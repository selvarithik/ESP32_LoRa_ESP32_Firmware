#pragma once
#include <stddef.h>
#include <stdint.h>

namespace NodeChat {
// Allow registration, one command ACK/response and the text receipt to use
// separate turns, even when text is queued during boot holdoff.
constexpr uint32_t deliveryDeadline(uint32_t pollInterval){return 8U*pollInterval;}
inline bool validText(const char* text,size_t size){
 if(!text||!size||size>24)return false;
 for(size_t i=0;i<size;++i)if(text[i]<32||text[i]>126)return false;
 return true;
}
class AccessWindow {
 uint32_t touched=0,blockedAt=0;
 unsigned failures=0;
 bool session=false;
public:
 bool locked(uint32_t now){
  if(failures<5)return false;
  if(now-blockedAt<60000)return true;
  failures=0;return false;
 }
 void fail(uint32_t now){if(++failures==5)blockedAt=now;}
 void login(uint32_t now){failures=0;touched=now;session=true;}
 bool authorize(uint32_t now,bool matches){
  if(!session||now-touched>=1800000){session=false;return false;}
  if(!matches)return false;
  touched=now;return true;
 }
};
#ifdef ARDUINO
}
#include <Arduino.h>
namespace NodeChat {
bool configurePin(const char* pin);
bool verifyPin(const char* pin);
bool submit(const char* text,size_t size);
String messages(uint32_t since);
#endif
}
