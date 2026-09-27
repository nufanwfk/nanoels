// SPDX-License-Identifier: MIT
// Copy with the three headers into openHASP/src/custom, not NanoELS/h5.
#include "hasplib.h"
#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
#include "hasp_debug.h"
#include "nanoels_bridge.h"
#include <HardwareSerial.h>

// Deliberately no DIS02050A pin guesses. Set these in user_config_override.h
// after checking the board port, schematics, and existing peripheral use.
#if !defined(ARDUINO_ARCH_ESP32)
#error "The NanoELS UART adapter requires Arduino ESP32"
#endif
#if !defined(NANOELS_UART_RX) || !defined(NANOELS_UART_TX) || !defined(NANOELS_UART_NUMBER)
#error "Define NANOELS_UART_RX, NANOELS_UART_TX and NANOELS_UART_NUMBER"
#endif
#if NANOELS_UART_NUMBER < 1 || NANOELS_UART_NUMBER > 2
#error "Use a dedicated UART 1 or 2; UART 0 is reserved for the console"
#endif
#if NANOELS_UART_RX < 0 || NANOELS_UART_TX < 0 || NANOELS_UART_RX == NANOELS_UART_TX
#error "Choose distinct valid RX and TX GPIOs for this board"
#endif

namespace {
HardwareSerial elsSerial(NANOELS_UART_NUMBER);
bool ready = false;

void displayCommand(void*, const char* command) {
    dispatch_text_line(command, TAG_CUSTOM);
}
void sendPacket(void*, const uint8_t* bytes, size_t count) {
    elsSerial.write(bytes, count);
}
nanoels::Bridge bridge({nullptr, displayCommand, sendPacket, nullptr});
} // namespace

void custom_setup() {
    elsSerial.setRxBufferSize(2048);
    elsSerial.begin(115200, SERIAL_8N1, NANOELS_UART_RX, NANOELS_UART_TX);
    ready = true;
    dispatch_text_line("page 1", TAG_CUSTOM);
}

void custom_loop() {
    if(!ready) return;
    // Limit work each iteration to let LVGL process releases and redraws.
    for(size_t budget = 0; budget < 256 && elsSerial.available(); ++budget) {
        const int value = elsSerial.read();
        if(value < 0) break;
        const uint8_t byte = static_cast<uint8_t>(value);
        bridge.feed(&byte, 1);
    }
}

void custom_state_subtopic(const char* subtopic, const char* payload) {
    if(!ready || !subtopic || !payload) return;
    // Page-state notifications also cover the local BACK action. H5-directed
    // page changes already release before dispatch; cancelTouch is idempotent.
    if(strcmp(subtopic, "page") == 0) { bridge.cancelTouch(); return; }
    if(subtopic[0] != 'p') return;
    StaticJsonDocument<256> doc;
    if(deserializeJson(doc, payload)) return;
    if(!doc["event"].is<const char*>()) return;
    bridge.touch(subtopic, doc["event"].as<const char*>());
}

bool custom_pin_in_use(uint8_t pin) {
    return pin == NANOELS_UART_RX || pin == NANOELS_UART_TX;
}
void custom_get_sensors(JsonDocument& doc) {
    doc["NanoELS"]["rejected_frames"] = bridge.rejectedFrames();
}
void custom_every_second() {}
void custom_every_5seconds() {}
void custom_topic_payload(const char*, const char*, uint8_t) {}
#endif
