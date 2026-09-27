// SPDX-License-Identifier: MIT
#include "nanoels_bridge.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

struct Harness {
    std::vector<std::string> commands;
    std::vector<std::vector<uint8_t>> packets;
    std::vector<std::string> order;
    unsigned beeps = 0;
    nanoels::Bridge bridge;
    Harness() : bridge({this, command, packet, beep}) {}
    static void command(void* self, const char* command) {
        static_cast<Harness*>(self)->commands.emplace_back(command);
        static_cast<Harness*>(self)->order.emplace_back(command);
    }
    static void packet(void* self, const uint8_t* data, size_t count) {
        static_cast<Harness*>(self)->packets.emplace_back(data, data + count);
        static_cast<Harness*>(self)->order.emplace_back(data[3] ? "press" : "release");
    }
    static void beep(void* self) { ++static_cast<Harness*>(self)->beeps; }
    void bytes(const std::string& text) {
        bridge.feed(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }
    void frame(const std::string& text) { bytes(text + std::string(3, '\xFF')); }
};

static void packetIs(const Harness& h, size_t index, int page, int id, bool down) {
    const std::vector<uint8_t> expected = {0x65, static_cast<uint8_t>(page),
        static_cast<uint8_t>(id), static_cast<uint8_t>(down), 0xFF, 0xFF, 0xFF};
    assert(h.packets.at(index) == expected);
}

static void framingAndText() {
    const std::string frame = "tZ.txt=\"0.125\"" + std::string(3, '\xFF');
    // Every possible two-chunk split, including inside the terminator.
    for(size_t split = 0; split <= frame.size(); ++split) {
        Harness h;
        h.bytes(frame.substr(0, split));
        if(split < frame.size()) assert(h.commands.empty());
        h.bytes(frame.substr(split));
        assert(h.commands.size() == 1);
        assert(h.commands[0] == "jsonl {\"page\":1,\"id\":21,\"text\":\"0.125\"}");
        assert(h.packets.empty());
    }
    Harness h;
    h.bytes(frame + frame);
    assert(h.commands.size() == 2);
    h.frame("t3.txt=\"\"");
    assert(h.commands.back() == "jsonl {\"page\":1,\"id\":2,\"text\":\"\"}");
    h.frame("tAngleVal.txt=\"90\xDF\"");
    assert(h.commands.back() == "jsonl {\"page\":1,\"id\":10,\"text\":\"90\xC2\xB0\"}");
    h.frame("t3.txt=\"A\\B\n\t\"");
    assert(h.commands.back() == "jsonl {\"page\":1,\"id\":2,\"text\":\"A\\\\B\\u000a\\u0009\"}");
    h.frame("play 0,0,0");
    assert(h.beeps == 1);
    h.frame("page 0");
    assert(h.commands.back() == "page 1");
    h.frame("page 1");
    assert(h.commands.back() == "page 2");
}

static void invalidFramesAndRecovery() {
    Harness h;
    const std::vector<std::string> invalid = {
        "page 2", "page 0;page 1", "page 00", "connect", "baud=9600",
        "whmi-wri 1024,115200,0", "unknown.txt=\"x\"", "bX.txt=\"x\"",
        "tZ.txt=", "tZ.txt=\"unterminated", "tZ.txt=\"x\"junk",
        "tZ.txt=\"x\";page 1;\"\"", "tZ.txt=\"\xFE\"",
        std::string("tZ.txt=\"x\0y\"", 12),
        std::string(513, 'A') + "page 1",
        std::string("noise\xFF\xFF") + "page 1"
    };
    for(const auto& frame : invalid) {
        const size_t before = h.commands.size();
        h.frame(frame);
        assert(h.commands.size() == before);
        h.frame("page 0");
        assert(h.commands.size() == before + 1);
    }
    assert(h.bridge.rejectedFrames() == invalid.size());
    h.frame("");
    assert(h.bridge.rejectedFrames() == invalid.size());
    assert(h.packets.empty());

    // Largest accepted command; worst-case JSON escaping fits the output buffer.
    Harness boundary;
    boundary.frame("t3.txt=\"" + std::string(502, '\x01') + "\""); // 511 bytes
    assert(boundary.commands.size() == 1);
    boundary.frame("t3.txt=\"" + std::string(503, '\x01') + "\""); // 512 bytes
    assert(boundary.commands.size() == 2);
    boundary.frame("t3.txt=\"" + std::string(504, '\x01') + "\""); // 513 bytes
    assert(boundary.commands.size() == 2);
    boundary.frame("page 0");
    assert(boundary.commands.back() == "page 1");
}

static void touchLifecycle() {
    Harness h;
    h.bridge.touch("p1b48", "down");
    packetIs(h, 0, 0, 48, true);
    h.bridge.touch("p1b48", "down");
    h.bridge.touch("p1b48", "long");
    h.bridge.touch("p1b48", "hold");
    h.bridge.touch("p1b48", "changed");
    assert(h.packets.size() == 1);
    h.bridge.touch("p1b48", "release");
    packetIs(h, 1, 0, 48, false);
    h.bridge.touch("p1b48", "up");
    assert(h.packets.size() == 2);

    // Numeric limit aliases use hotspot IDs, not the displayed object's ID.
    h.bridge.touch("p1b13", "down");
    packetIs(h, 2, 0, 36, true);
    h.bridge.touch("p1b13", "up");
    packetIs(h, 3, 0, 36, false);

    // Release using the original mode page, even when H5 changes page on DOWN.
    h.bridge.touch("p2b17", "down");
    h.frame("page 0");
    packetIs(h, 4, 1, 17, true);
    packetIs(h, 5, 1, 17, false);
    assert(h.order[h.order.size() - 2] == "release");
    assert(h.order.back() == "page 1");
    h.bridge.touch("p2b17", "release");
    assert(h.packets.size() == 6);

    h.bridge.touch("p1b48", "down");
    h.bridge.touch("p1b49", "down");
    packetIs(h, 7, 0, 48, false);
    packetIs(h, 8, 0, 49, true);
    h.bridge.touch("p1b48", "release"); // stale release
    assert(h.packets.size() == 9);
    h.bridge.touch("p1b49", "lost");
    packetIs(h, 9, 0, 49, false);
    h.bridge.cancelTouch();
    assert(h.packets.size() == 10);

    for(const char* topic : {"p2b100", "p1b2", "p0b48", "p1b255", "p1b48junk", "page"}) {
        h.bridge.touch(topic, "down");
        h.bridge.touch(topic, "up");
    }
    h.bridge.touch(nullptr, "down");
    h.bridge.touch("p1b48", nullptr);
    assert(h.packets.size() == 10);
}

static void allMappings() {
    assert(sizeof(nanoels::kFields) / sizeof(nanoels::kFields[0]) == 18);
    assert(sizeof(nanoels::kTouches) / sizeof(nanoels::kTouches[0]) == 60);
    for(const auto& field : nanoels::kFields) {
        Harness h;
        h.frame(std::string(field.name) + ".txt=\"test\"");
        const std::string expected = "jsonl {\"page\":" + std::to_string(field.page) +
            ",\"id\":" + std::to_string(field.id) + ",\"text\":\"test\"}";
        assert(h.commands.size() == 1 && h.commands[0] == expected);
    }
    for(const auto& key : nanoels::kTouches) {
        Harness h;
        h.bridge.touch(key.topic, "down");
        h.bridge.touch(key.topic, "up");
        assert(h.packets.size() == 2);
        packetIs(h, 0, key.page, key.id, true);
        packetIs(h, 1, key.page, key.id, false);
    }
}

int main() {
    framingAndText();
    invalidFramesAndRecovery();
    touchLifecycle();
    allMappings();
    std::cout << "PASS: framing, translation, rejection/recovery, touch lifecycle, all mappings\n";
}
