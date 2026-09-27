// SPDX-License-Identifier: MIT
// Compile H5's actual adapter functions with a fake UART and mutex. No motor code.
#include "display_protocol.h"
#include <assert.h>
#include <deque>
#include <iostream>
#include <string>
using byte = uint8_t;
using String = std::string;
using SemaphoreHandle_t = void*;
constexpr int portMAX_DELAY = 0;
constexpr int PS2_BREAK = 0x8000;
bool locked = false;
void xSemaphoreTake(SemaphoreHandle_t mutex, int) { assert(mutex && !locked); locked = true; }
void xSemaphoreGive(SemaphoreHandle_t) { assert(locked); locked = false; }
struct SerialMock {
  std::deque<byte> input;
  std::string output;
  int available() { return int(input.size()); }
  int read() { if(input.empty()) return -1; int b=input.front();input.pop_front();return b; }
  size_t write(const uint8_t* data, size_t n) {
    assert(locked); output.append(reinterpret_cast<const char*>(data),n);return n;
  }
  void inject(const std::string& text) { for(byte b:text) input.push_back(b); }
} Serial1;
bool tftUploadActive = false;
// Generated from h5.ino by run_display_tests.py, not reimplementations.
#include "h5_display_functions.inc"
#include "display_test_cases.inc"

int main() {
  screenTxMutex = reinterpret_cast<void*>(1);
  for(const auto& test : textCases) {
    activeDisplayBackend = h5display::Nextion;
    Serial1.output.clear(); setText(test.name,"12.3");
    assert(Serial1.output == std::string(test.name)+".txt=\"12.3\"\xFF\xFF\xFF");
    activeDisplayBackend = h5display::OpenHasp;
    Serial1.output.clear(); setText(test.name,"12.3");
    assert(Serial1.output == "jsonl {\"page\":"+std::to_string(test.page)+",\"id\":"+
      std::to_string(test.id)+",\"text\":\"12.3\"}\n");
  }
  for(const auto& test : touchCases) {
    for(auto backend : {h5display::Nextion, h5display::OpenHasp}) {
      activeDisplayBackend = backend;
      if(backend == h5display::Nextion) {
        const byte bytes[]={0x65,test.page,test.id,1,255,255,255,0x65,test.page,test.id,0,255,255,255};
        Serial1.inject(std::string(reinterpret_cast<const char*>(bytes),sizeof(bytes)));
      } else {
        Serial1.inject(std::string(test.topic)+" {\"event\":\"down\"}\n"+test.topic+" {\"event\":\"release\"}\n");
      }
      assert(readScreenEvent()==test.action);
      assert(lastScreenPageId==test.page);
      assert(readScreenEvent()==(test.action|PS2_BREAK));
      assert(readScreenEvent()==0);
    }
  }
  activeDisplayBackend=h5display::OpenHasp;
  Serial1.inject("p1b48 {\"event\":\"down\"}\n");
  assert(readScreenEvent()==B_LEFT);
  setScreenPage(1);
  assert(readScreenEvent()==(B_LEFT|PS2_BREAK));
  assert(lastScreenPageId==0); // release retains the old page
  Serial1.inject("p1b48 {\"event\":\"release\"}\n");
  assert(readScreenEvent()==0); // already released by the page change

  Serial1.inject("p2b17 {\"event\":\"down\"}\nready 1\n");
  assert(readScreenEvent()==B_MODE_THREAD);
  assert(readScreenEvent()==(B_MODE_THREAD|PS2_BREAK));
  Serial1.output.clear();
  assert(refreshScreenIfRequested());
  assert(Serial1.output=="page 2\n");
  assert(!refreshScreenIfRequested());

  Serial1.inject("page 1\nready 1\n");
  assert(readScreenEvent()==0);
  Serial1.output.clear(); assert(refreshScreenIfRequested());
  assert(Serial1.output=="page 1\n");

  Serial1.inject("p1b48 {\"event\":\"down\"}\np1b49 {\"event\":\"down\"}\n");
  assert(readScreenEvent()==B_LEFT);
  assert(readScreenEvent()==(B_LEFT|PS2_BREAK));
  assert(readScreenEvent()==B_RIGHT);
  Serial1.inject("p1b48 {\"event\":\"up\"}\np1b49 {\"event\":\"up\"}\n");
  assert(readScreenEvent()==(B_RIGHT|PS2_BREAK));
  assert(readScreenEvent()==0);

  Serial1.output.clear(); beep(); assert(Serial1.output.empty());
  activeDisplayBackend=h5display::Nextion;
  beep(); assert(Serial1.output=="play 0,0,0\xFF\xFF\xFF");
  Serial1.output.clear(); tftUploadActive=true;
  setText("tZ","12"); setScreenPage(0); beep(); assert(Serial1.output.empty());
  std::cout << "PASS: H5 functions, 18 fields, 60 widgets on both backends, page/restart releases, refresh and upload suppression\n";
}
