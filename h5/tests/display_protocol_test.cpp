// SPDX-License-Identifier: MIT
#include "display_protocol.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <vector>
using namespace h5display;

std::vector<Event> receive(Receiver& rx, Backend backend, const std::string& bytes) {
  std::vector<Event> events;
  for(uint8_t b : bytes) { Event e; if(rx.feed(backend, b, e)) events.push_back(e); }
  return events;
}
void formats() {
  char out[1024];
  auto n = textCommand(Nextion, "tZ", "0.125", out, sizeof(out));
  assert(std::string(out, n) == "tZ.txt=\"0.125\"\xFF\xFF\xFF");
  n = textCommand(OpenHasp, "tZ", "0.125", out, sizeof(out));
  assert(std::string(out, n) == "jsonl {\"page\":1,\"id\":21,\"text\":\"0.125\"}\n");
  n = textCommand(OpenHasp, "tAngleVal", "90\xDF", out, sizeof(out));
  assert(std::string(out, n).find("90\xC2\xB0") != std::string::npos);
  n = textCommand(Nextion, "tAngleVal", "90\xDF", out, sizeof(out));
  assert(std::string(out, n).find("90\xDF") != std::string::npos);
  n = textCommand(OpenHasp, "t3", "\"A\\B\n\r\t", out, sizeof(out));
  assert(std::string(out,n) == "jsonl {\"page\":1,\"id\":2,\"text\":\"\\\"A\\\\B\\u000a\\u000d\\u0009\"}\n");
  assert(textCommand(OpenHasp,"unknown","x",out,sizeof(out)) == 0);
  assert(textCommand(OpenHasp,"tZ",std::string(2000,'x').c_str(),out,sizeof(out)) == 0);
  assert(textCommand(OpenHasp,"tZ","x",out,1) == 0);
  for(int p = 0; p < 2; ++p) {
    n = pageCommand(Nextion,p,out,sizeof(out));
    assert(std::string(out,n) == "page " + std::to_string(p) + "\xFF\xFF\xFF");
    n = pageCommand(OpenHasp,p,out,sizeof(out));
    assert(std::string(out,n) == "page " + std::to_string(p+1) + "\n");
  }
  assert(pageCommand(OpenHasp,2,out,sizeof(out)) == 0);
  assert(beepCommand(OpenHasp,out,sizeof(out)) == 0);
  n = beepCommand(Nextion,out,sizeof(out));
  assert(std::string(out,n) == "play 0,0,0\xFF\xFF\xFF");
}
void events() {
  Receiver rx;
  const std::string msg = "event p1b48 down\r\n";
  for(size_t i = 0; i <= msg.size(); ++i) {
    rx.reset();
    auto a = receive(rx,OpenHasp,msg.substr(0,i));
    auto b = receive(rx,OpenHasp,msg.substr(i));
    assert(a.size()+b.size() == 1);
    const auto e = a.empty() ? b[0] : a[0];
    assert(e.page == 0 && e.id == 48 && e.down);
  }
  auto a = receive(rx,OpenHasp,"event p1b13 down\nevent p1b13 up\n");
  assert(a.size() == 2 && a[0].id == 36 && a[0].down && !a[1].down);
  a=receive(rx,OpenHasp,"ready 1\npage 1\npage 2\n");
  assert(a.size()==3 && a[0].kind==Ready && a[1].page==0 && a[2].page==1);
  for(const char* ev : {"long","hold","changed","UP","bogus"})
    assert(receive(rx,OpenHasp,std::string("event p1b48 ")+ev+"\n").empty());
  for(const char* ev : {"long","hold","changed"})
    assert(receive(rx,OpenHasp,std::string("event p1b254 ")+ev+"\n").empty());
  for(const char* topic : {"p2b100","p1b2","p0b48"}) {
    auto unknown = receive(rx,OpenHasp,std::string("event ")+topic+" down\n");
    assert(unknown.size() == 1 && unknown[0].kind == UnknownTouch && unknown[0].down);
  }
  for(const char* topic : {"p1b255","p1b48junk","p13b48"})
    assert(receive(rx,OpenHasp,std::string("event ")+topic+" down\n").empty());
  const std::vector<std::string> bad = {
    "event", "event p1b48", "event p1b48 ", "event p1b48 down junk",
    "event  p1b48 down", "event p1b48  down", "event p1b48 down ",
    "event\tp1b48 down", "Event p1b48 down", "event p1b48 down\rup",
    "p1b48 {\"event\":\"down\"}", "page 3", "ready 0"
  };
  for(const auto& line : bad) {
    assert(receive(rx,OpenHasp,line+"\n").empty());
    assert(receive(rx,OpenHasp,"event p1b48 release\n").size()==1);
  }
  for(const char* ev : {"up","release","lost"}) {
    auto released = receive(rx,OpenHasp,std::string("event p1b48 ")+ev+"\n");
    assert(released.size()==1 && !released[0].down);
  }
  assert(receive(rx,OpenHasp,std::string(513,'x')+"event p1b48 down\n").empty());
  assert(receive(rx,OpenHasp,std::string("event p1b48 ")+std::string(1,'\0')+"down\n").empty());
  assert(receive(rx,OpenHasp,"event p1b48 down\n").size()==1);

  std::string nx = std::string("\x65\x00\x30\x01",4)+"\xFF\xFF\xFF";
  a=receive(rx,Nextion,nx);
  assert(a.size()==1 && a[0].id==48 && a[0].down);
  nx[3]=0;
  assert(!receive(rx,Nextion,nx)[0].down);
  nx[3]=2;
  assert(receive(rx,Nextion,nx).empty());
  assert(receive(rx,Nextion,std::string(600,'x')+"\xFF\xFF\xFF").empty());
  nx[3]=1; assert(receive(rx,Nextion,nx).size()==1);
  assert(receive(rx,Nextion,std::string("\x88\xFF\xFF\xFF",4)).empty());
}
void lifecycle() {
  Receiver rx; TouchState held; Event out[2];
  auto down=receive(rx,OpenHasp,"event p1b48 down\n")[0];
  assert(held.accept(down,out)==1 && out[0].down);
  assert(held.accept(down,out)==0);
  auto newer=receive(rx,OpenHasp,"event p1b49 down\n")[0];
  assert(held.accept(newer,out)==2 && !out[0].down && out[0].id==48 && out[1].id==49);
  auto unknown=receive(rx,OpenHasp,"event p1b254 down\n")[0];
  assert(unknown.kind == UnknownTouch && unknown.down);
  assert(held.cancel(out[0]) && !out[0].down && out[0].id==49);
  auto unknownRelease=receive(rx,OpenHasp,"event p1b254 release\n")[0];
  assert(unknownRelease.kind == UnknownTouch && !unknownRelease.down);
  assert(!held.cancel(out[0]));
  assert(held.accept(newer,out)==1 && out[0].down && out[0].id==49);
  down.down=false; assert(held.accept(down,out)==0);
  assert(held.cancel(out[0]) && !out[0].down && out[0].id==49);
  assert(!held.cancel(out[0]));
  newer.down=false; assert(held.accept(newer,out)==0);
}
int main() {
  formats(); events(); lifecycle();
  std::cout << "PASS: both wire formats, framing, bounded event parsing and touch lifecycle\n";
}
