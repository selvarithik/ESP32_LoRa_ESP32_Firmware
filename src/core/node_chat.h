#pragma once
#include <stddef.h>
#include <stdint.h>

namespace NodeChat {
inline bool validText(const char* text,size_t size){
 if(!text||!size||size>24)return false;
 for(size_t i=0;i<size;++i)if(text[i]<32||text[i]>126)return false;
 return true;
}
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
