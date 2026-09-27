// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nanoels_mapping.h"

namespace nanoels {

// Callbacks are synchronous; consume/copy buffers before returning. Use one
// thread (openHASP's main loop) for feed(), touch(), and cancelTouch().
struct Callbacks {
    void* context;
    void (*command)(void*, const char*); // openHASP text command, no newline
    void (*packet)(void*, const uint8_t*, size_t); // binary H5 UART output
    void (*beep)(void*); // optional; nullptr means silent
};

class Bridge {
public:
    static const size_t kMaxCommand = 512; // excludes FF FF FF terminator

    explicit Bridge(Callbacks callbacks) : callbacks_(callbacks) {}

    // Feed arbitrary UART chunks, including partial or multiple frames.
    void feed(const uint8_t* data, size_t count) {
        for(size_t i = 0; i < count; ++i) feedByte(data[i]);
    }

    // Exact allowlisted pNbN topics only; tags are not trusted as packet IDs.
    void touch(const char* topic, const char* event) {
        if(!topic || !event) return;
        const Touch* target = nullptr;
        for(const auto& candidate : kTouches) {
            if(strcmp(topic, candidate.topic) == 0) { target = &candidate; break; }
        }
        if(!target) return; // Includes local BACK and non-action labels.
        if(strcmp(event, "down") == 0) {
            if(active_ == target) return; // Duplicate down is not a new press.
            cancelTouch(); // Never leave a previous key latched.
            active_ = target;
            sendTouch(*target, true);
        } else if(strcmp(event, "up") == 0 || strcmp(event, "release") == 0 ||
                  strcmp(event, "lost") == 0) {
            // A delayed release from another widget must not stop a newer press.
            if(active_ == target) cancelTouch();
        }
        // long/hold/changed and unknown event types never repeat machine actions.
    }

    // Call before destroying/reloading objects, changing pages locally, or
    // intentionally shutting down UART. This cannot recover a broken wire.
    void cancelTouch() {
        if(!active_) return;
        const Touch* previous = active_;
        active_ = nullptr; // Clear before callback to avoid a reentrant release.
        sendTouch(*previous, false);
    }

    uint32_t rejectedFrames() const { return rejected_; }

private:
    Callbacks callbacks_;
    char frame_[kMaxCommand + 1] = {};
    // Worst case: every input byte becomes a six-character JSON escape.
    char output_[kMaxCommand * 6 + 80] = {};
    size_t length_ = 0;
    uint8_t terminators_ = 0;
    bool dropping_ = false;
    const Touch* active_ = nullptr;
    uint32_t rejected_ = 0;

    void sendTouch(const Touch& key, bool down) {
        const uint8_t bytes[] = {0x65, key.page, key.id,
                                static_cast<uint8_t>(down), 0xFF, 0xFF, 0xFF};
        if(callbacks_.packet) callbacks_.packet(callbacks_.context, bytes, sizeof(bytes));
    }

    void feedByte(uint8_t byte) {
        if(byte == 0xFF) {
            if(++terminators_ == 3) {
                if(dropping_) ++rejected_;
                else if(length_) {
                    frame_[length_] = '\0';
                    if(!translate()) ++rejected_;
                }
                length_ = 0;
                terminators_ = 0;
                dropping_ = false;
            }
            return;
        }
        // Isolated FF bytes are not valid text in H5's command subset. Drop the
        // entire damaged/oversized frame, never parse its tail as a new command.
        if(terminators_) dropping_ = true;
        terminators_ = 0;
        if(dropping_) return;
        if(byte == 0 || length_ == kMaxCommand) { dropping_ = true; return; }
        frame_[length_++] = static_cast<char>(byte);
    }

    void emit(const char* command) {
        if(callbacks_.command) callbacks_.command(callbacks_.context, command);
    }

    bool translate() {
        if(strcmp(frame_, "play 0,0,0") == 0) {
            if(callbacks_.beep) callbacks_.beep(callbacks_.context);
            return true;
        }
        for(const auto& page : kPages) {
            char expected[16];
            snprintf(expected, sizeof(expected), "page %u", page.nextion);
            if(strcmp(frame_, expected) == 0) {
                // LVGL may suppress release when the old screen disappears.
                // Send it with the ORIGINAL key/page before switching screens.
                cancelTouch();
                snprintf(output_, sizeof(output_), "page %u", page.openhasp);
                emit(output_);
                return true;
            }
        }
        for(const auto& field : kFields) {
            const size_t nameLength = strlen(field.name);
            if(length_ < nameLength + 7 ||
               strncmp(frame_, field.name, nameLength) != 0 ||
               strncmp(frame_ + nameLength, ".txt=\"", 6) != 0 ||
               frame_[length_ - 1] != '"') continue;
            const size_t begin = nameLength + 6;
            const size_t end = length_ - 1;
            // H5 emits ASCII plus its font's byte DF for degrees. Reject other
            // encodings instead of passing invalid UTF-8 to the display.
            for(size_t i = begin; i < end; ++i) {
                const uint8_t c = static_cast<uint8_t>(frame_[i]);
                if(c == '"' || (c >= 0x80 && c != 0xDF)) return false;
            }
            size_t out = static_cast<size_t>(snprintf(output_, sizeof(output_),
                "jsonl {\"page\":%u,\"id\":%u,\"text\":\"", field.page, field.id));
            const char* hex = "0123456789abcdef";
            for(size_t i = begin; i < end; ++i) {
                const uint8_t c = static_cast<uint8_t>(frame_[i]);
                if(c == 0xDF) {
                    output_[out++] = static_cast<char>(0xC2);
                    output_[out++] = static_cast<char>(0xB0);
                } else if(c == '\\') {
                    output_[out++] = '\\'; output_[out++] = '\\';
                } else if(c < 0x20 || c == 0x7F) {
                    output_[out++] = '\\'; output_[out++] = 'u';
                    output_[out++] = '0'; output_[out++] = '0';
                    output_[out++] = hex[c >> 4]; output_[out++] = hex[c & 15];
                } else output_[out++] = static_cast<char>(c);
            }
            output_[out++] = '"'; output_[out++] = '}'; output_[out] = '\0';
            emit(output_);
            return true;
        }
        return false; // Includes connect, baud changes and TFT upload commands.
    }
};
} // namespace nanoels
