// SPDX-License-Identifier: MIT
// H5-side wire formats. No Arduino, graphics, or JSON-library dependency.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "display_mapping.h"

namespace h5display {
enum Backend { Nextion = 0, OpenHasp = 1 };
enum Kind { None, Touch, Ready, Page };
struct Event {
  Kind kind = None;
  uint8_t page = 0, id = 0; // Existing H5/Nextion page and component identity.
  uint16_t source = 0;      // Original openHASP widget, including limit aliases.
  bool down = false;
};

// Bounded writer: a truncated command is never sent to the display.
class Writer {
public:
  Writer(char* output, size_t capacity) : output_(output), capacity_(capacity) {}
  void byte(char c) {
    if(size_ < capacity_) output_[size_] = c;
    ++size_;
  }
  void text(const char* text) { while(*text) byte(*text++); }
  size_t finish() {
    if(size_ >= capacity_) return 0;
    output_[size_] = 0;
    return size_;
  }
private:
  char* output_; size_t capacity_, size_ = 0;
};

inline size_t textCommand(Backend backend, const char* name, const char* text,
                          char* output, size_t capacity) {
  const Field* field = nullptr;
  for(const auto& f : fields) if(strcmp(name, f.name) == 0) { field = &f; break; }
  if(!field) return 0;
  Writer out(output, capacity);
  if(backend == Nextion) {
    out.text(name); out.text(".txt=\""); out.text(text); out.byte('"');
    out.text("\xFF\xFF\xFF");
  } else {
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "jsonl {\"page\":%u,\"id\":%u,\"text\":\"", field->page, field->id);
    out.text(prefix);
    const char* hex = "0123456789abcdef";
    while(*text) {
      const uint8_t c = static_cast<uint8_t>(*text++);
      if(c == 0xDF) out.text("\xC2\xB0"); // H5's Nextion font degree glyph.
      else if(c == '"' || c == '\\') { out.byte('\\'); out.byte(static_cast<char>(c)); }
      else if(c < 0x20) { out.text("\\u00"); out.byte(hex[c >> 4]); out.byte(hex[c & 15]); }
      else out.byte(static_cast<char>(c));
    }
    out.text("\"}\n");
  }
  return out.finish();
}

inline size_t pageCommand(Backend backend, uint8_t page, char* output, size_t capacity) {
  if(page > 1) return 0;
  Writer out(output, capacity);
  out.text("page "); out.byte('0' + page + (backend == OpenHasp ? 1 : 0));
  out.text(backend == Nextion ? "\xFF\xFF\xFF" : "\n");
  return out.finish();
}

inline size_t beepCommand(Backend backend, char* output, size_t capacity) {
  if(backend != Nextion) return 0; // No portable openHASP audio command.
  Writer out(output, capacity); out.text("play 0,0,0\xFF\xFF\xFF"); return out.finish();
}

class Receiver {
public:
  static const size_t capacity = 512;
  void reset() { length_ = 0; terminators_ = 0; dropping_ = false; }
  bool feed(Backend backend, uint8_t byte, Event& event) {
    event = Event();
    if(backend == Nextion) {
      // Preserve all binary bytes; only complete, seven-byte touch packets count.
      if(length_ < sizeof(buffer_)) buffer_[length_++] = static_cast<char>(byte);
      else dropping_ = true;
      terminators_ = byte == 0xFF ? terminators_ + 1 : 0;
      if(terminators_ != 3) return false;
      const bool valid = !dropping_ && length_ == 7 &&
          static_cast<uint8_t>(buffer_[0]) == 0x65 &&
          static_cast<uint8_t>(buffer_[1]) <= 1 &&
          static_cast<uint8_t>(buffer_[3]) <= 1;
      if(valid) {
        event.kind = Touch; event.page = buffer_[1]; event.id = static_cast<uint8_t>(buffer_[2]);
        event.down = buffer_[3] == 1; event.source = event.page * 256 + event.id;
      }
      reset(); return valid;
    }
    if(byte == '\n') {
      bool valid = false;
      if(!dropping_ && length_) {
        if(buffer_[length_ - 1] == '\r') --length_;
        buffer_[length_] = 0;
        valid = parseLine(event);
      }
      reset(); return valid;
    }
    if(byte == 0 || length_ >= capacity) dropping_ = true;
    if(!dropping_) buffer_[length_++] = static_cast<char>(byte);
    return false;
  }
private:
  char buffer_[capacity + 1] = {};
  size_t length_ = 0;
  uint8_t terminators_ = 0;
  bool dropping_ = false;
  bool parseLine(Event& event) {
    // Versioned, transport-only ready notification; no machine action implied.
    if(strcmp(buffer_, "ready 1") == 0) { event.kind = Ready; return true; }
    if(strcmp(buffer_, "page 1") == 0 || strcmp(buffer_, "page 2") == 0) {
      event.kind = Page; event.page = buffer_[5] - '1'; return true;
    }
    if(strncmp(buffer_, "event ", 6) != 0) return false;
    char* topic = buffer_ + 6;
    char* separator = strchr(topic, ' ');
    if(!separator) return false;
    *separator++ = 0;
    const Widget* widget = nullptr;
    for(const auto& w : widgets) if(strcmp(topic, w.topic) == 0) { widget = &w; break; }
    if(!widget) return false;
    const char* value = separator;
    const bool down = strcmp(value, "down") == 0;
    if(!down && strcmp(value, "up") && strcmp(value, "release") && strcmp(value, "lost")) return false;
    event.kind = Touch; event.page = widget->page; event.id = widget->id;
    event.source = widget->source; event.down = down; return true;
  }
};

// Used only for openHASP. At most one held widget; extra metadata has no authority.
class TouchState {
public:
  unsigned accept(const Event& event, Event (&output)[2]) {
    if(event.down) {
      if(active_.kind == Touch && active_.source == event.source) return 0;
      unsigned n = cancel(output[0]) ? 1 : 0;
      active_ = event; output[n++] = event; return n;
    }
    if(active_.kind != Touch || active_.source != event.source) return 0;
    return cancel(output[0]) ? 1 : 0;
  }
  bool cancel(Event& event) {
    if(active_.kind != Touch) return false;
    event = active_; event.down = false; active_ = Event(); return true;
  }
private:
  Event active_;
};
} // namespace h5display
