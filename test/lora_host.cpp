// Historical harness for the retired newline UART driver; not a current test target.
// Current native checks: check_e220.py; current full driver builds: PlatformIO.
#include "../src/communication/lora_transport.h"
#include "../src/core/logger.h"
#include <assert.h>
#include <stdio.h>
uint32_t hostMillis=0;int hostAux=1;std::string HostUart::rx;int HostUart::room=128,HostUart::writes=0;bool HostUart::ended=false;
Logger logger;void Logger::info(const String&,const String&){}void Logger::warn(const String&,const String&){}
unsigned parsed=0;String Message::toJson()const{return payload;}bool Message::fromJson(const String& s,Message&){++parsed;return s=="valid";}
int main(){
 DeviceConfig c;LoRaTransport radio;assert(!radio.begin(c));c.lora_enabled=true;c.lora_uart=1;assert(!radio.begin(c));c.lora_uart=2;
 hostAux=0;assert(!radio.begin(c));assert(!radio.isInitialized());assert(HostUart::ended);assert(hostMillis==1500);
 hostAux=1;assert(radio.begin(c));Message m;m.payload="valid";HostUart::room=2;assert(!radio.send(m));assert(HostUart::writes==0);
 HostUart::room=128;hostAux=0;assert(!radio.send(m));hostAux=1;assert(radio.send(m));assert(HostUart::writes==1);
 HostUart::rx=std::string(4097,'x')+"valid\n";unsigned calls=0;while(radio.available()){assert(!radio.receive(m));assert(++calls<40);}assert(parsed==0);
 HostUart::rx="valid\n";assert(radio.receive(m));assert(parsed==1);
 puts("PASS actual E220 UART branch with host doubles: disabled/unsupported UART, AUX timeout, queue pressure, bounded receive, overflow-tail discard and subsequent frame recovery; no hardware test");
}
