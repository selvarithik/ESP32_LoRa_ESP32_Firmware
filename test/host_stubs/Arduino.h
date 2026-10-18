#pragma once
// Native test doubles only; no physical pins/UART are accessed.
#include <stdint.h>
#include <stddef.h>
#include <string>
class String:public std::string {public:using std::string::string;using std::string::operator=;void trim(){auto a=find_first_not_of(" \r\n\t"),b=find_last_not_of(" \r\n\t");if(a==npos)clear();else assign(substr(a,b-a+1));}};
struct HostUart {static std::string rx;static int room,writes;static bool ended;};
class HardwareSerial {public:HardwareSerial(int){}void begin(int,int,int,int){}void end(){HostUart::ended=true;}int available(){return HostUart::rx.size();}int read(){char c=HostUart::rx.front();HostUart::rx.erase(0,1);return c;}int availableForWrite(){return HostUart::room;}size_t println(const String& s){++HostUart::writes;return s.length()+2;}};
extern uint32_t hostMillis;extern int hostAux;
inline uint32_t millis(){return hostMillis;}inline void delay(int ms){hostMillis+=ms;}inline void pinMode(int,int){}inline void digitalWrite(int,int){}inline int digitalRead(int){return hostAux;}
#define INPUT_PULLUP 1
#define OUTPUT 2
#define LOW 0
#define HIGH 1
#define SERIAL_8N1 0
