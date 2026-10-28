#pragma once
#include <stdint.h>
#include <stddef.h>
namespace HttpBodyCore {
// Canonical JSON peers provide Content-Length. Unknown/chunked bodies fail closed.
template<class Client,class Sink,class Clock,class Pause>
bool read(Client& client,Sink& sink,int expected,size_t maximum,uint32_t timeout,Clock clock,Pause pause){
 if(expected<=0||size_t(expected)>maximum||!timeout)return false;
 const uint32_t started=clock();size_t received=0;uint8_t buffer[256];
 while(received<size_t(expected)){
  if(uint32_t(clock()-started)>=timeout)return false;
  int available=client.available();
  if(available>0){
   size_t count=size_t(expected)-received;if(count>sizeof(buffer))count=sizeof(buffer);if(count>size_t(available))count=available;
   int got=client.read(buffer,count);
   if(got<0)return false;
   if(got>0){if(size_t(got)>count||sink.write(buffer,size_t(got))!=size_t(got))return false;received+=size_t(got);continue;}
  }
  if(!client.connected())return false;
  pause();
 }
 return true;
}
}
