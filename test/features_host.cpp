#include "../src/core/feature_core.h"
#include "../src/core/indicator_core.h"
#include "../src/drivers/mock_hardware.h"
#include "../src/protocol/radio_frame.h"
#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <iomanip>
int main(int argc,char** argv){
 if(argc==3&&std::string(argv[1])=="radio"){std::string hex=argv[2];if(hex.size()%2||hex.size()>480)return 2;uint8_t raw[240];size_t n=hex.size()/2;for(size_t i=0;i<n;++i){char byte[3]={hex[i*2],hex[i*2+1],0};raw[i]=strtoul(byte,nullptr,16);}RadioFrame::Frame f;if(!RadioFrame::decode(raw,n,f))return 2;uint8_t out[240];size_t count=RadioFrame::encode(f,out,240);for(size_t i=0;i<count;++i)std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(out[i]);return 0;}
 if(argc==7&&std::string(argv[1])=="network"){std::cout<<FeatureCore::staticNetwork(argv[2],argv[3],argv[4],argv[5],argv[6]);return 0;}
 if(argc==8&&std::string(argv[1])=="sleep"){FeatureCore::Schedule s;s.enabled=atoi(argv[3]);s.start=atoi(argv[4]);s.duration=atoi(argv[5]);s.days=atoi(argv[6]);s.offset=atoi(argv[7]);std::cout<<FeatureCore::sleepRemaining(s,atoll(argv[2]));return 0;}
 assert(IndicatorCore::receiptTimeout(true,30)==15000);assert(IndicatorCore::receiptTimeout(false,30)==65000);
 assert(IndicatorCore::connected(true,60000,1000,65000));assert(!IndicatorCore::connected(true,66000,1000,65000));
 assert(!IndicatorCore::connected(true,1,0));assert(!IndicatorCore::connected(false,1000,900));assert(IndicatorCore::connected(true,1000,900));
 assert(IndicatorCore::connected(true,15999,1000));assert(!IndicatorCore::connected(true,16000,1000));
 assert(IndicatorCore::connected(true,100,0xfffffff0));assert(!IndicatorCore::connected(true,16000,0xfffffff0));
 assert(IndicatorCore::pinConflict(true,true,true,18,true,false,16,17,27,25,26));
 assert(!IndicatorCore::pinConflict(false,true,true,18,true,false,16,17,27,25,26));
 assert(IndicatorCore::pinConflict(true,true,false,4,true,true,18,17,27,25,26));
 assert(IndicatorCore::pinConflict(true,false,false,4,true,true,2,17,27,25,26));
 assert(!IndicatorCore::pinConflict(true,true,true,4,true,false,16,17,27,25,26));
 assert(std::string(IndicatorCore::screen(false,false,0,0,0,0).text)=="noFI");
 assert(std::string(IndicatorCore::screen(true,false,1000,0,0,-67).text)=="Conn");
 assert(std::string(IndicatorCore::screen(true,false,3000,0,0,-67).text)=="C003");
 assert(std::string(IndicatorCore::screen(true,true,6000,5500,0,-67).text)==" -67");
 assert(std::string(IndicatorCore::screen(true,true,3000,2900,0,-100,1000).text)=="00:02");
 assert(std::string(IndicatorCore::screen(true,true,6000,5500,0,-100).text)=="-100");
 assert(std::string(IndicatorCore::screen(true,false,1000000000,0,0,-67).text)=="C999");
 assert(IndicatorCore::screen(true,true,3000,2900,0,-67,1000).segments[1]&0x80);
 assert(std::string(IndicatorCore::screen(true,true,10005000,10004500,0,-67,0).text)=="99:59");
 for(unsigned t=0;t<30000;t+=1000)assert(IndicatorCore::screen(true,true,t,t,0,-67,0).text[0]!='n');
 assert(FeatureCore::staticNetwork("192.168.1.20","255.255.255.0","192.168.1.1","1.1.1.1","8.8.8.8"));
 assert(!FeatureCore::staticNetwork("192.168.1.0","255.255.255.0","192.168.1.1","1.1.1.1",""));
 assert(!FeatureCore::staticNetwork("192.168.1.2","255.0.255.0","192.168.1.1","1.1.1.1",""));
 FeatureCore::Schedule s;s.enabled=true;s.start=23*60;s.duration=7200;s.offset=0;int64_t midnight=1700006400;
 assert(FeatureCore::sleepRemaining(s,midnight+1800)==1800);assert(FeatureCore::sleepRemaining(s,midnight+3600)==0);assert(FeatureCore::sleepRemaining(s,1)==0);
 s.days=0;assert(!FeatureCore::valid(s));s.days=127;s.offset=-721;assert(!FeatureCore::valid(s));
 Hardware::MockSensor sensor;assert(sensor.read().simulated);sensor.next.valid=false;assert(!sensor.read().valid);
 Hardware::MockNetwork network;assert(!network.state().connected&&!network.state().internetKnown);network.reconnect();assert(network.state().connected&&!network.state().internet);network.disconnect();assert(!network.state().connected);
 RadioFrame::Frame f;f.type=1;f.ttl=4;f.sequence=123;strcpy(f.source,"SIM-MASTER");strcpy(f.destination,"SIM-NODE-1");memcpy(f.payload,"HELLO",5);f.length=5;uint8_t bytes[RadioFrame::MaxBytes];size_t n=RadioFrame::encode(f,bytes,sizeof(bytes));assert(n);
 Hardware::MockRadio radio;assert(!radio.send(bytes,n));assert(radio.begin());assert(radio.send(bytes,n));uint8_t received[240];assert(radio.receive(received,sizeof(received))==n);RadioFrame::Frame decoded;assert(RadioFrame::decode(received,n,decoded));assert(decoded.sequence==123&&std::string(decoded.destination)=="SIM-NODE-1");received[n-1]^=1;assert(!RadioFrame::decode(received,n,decoded));radio.drop=true;assert(radio.send(bytes,n));assert(!radio.receive(received,sizeof(received)));assert(radio.metrics().transmitted==2&&radio.metrics().received==1);
 assert(FeatureCore::powerDecision(false,0,true,true,100,1,true,true)==FeatureCore::PowerDecision::DRAINING);
 assert(FeatureCore::powerDecision(false,0,true,false,0,0,true,true)==FeatureCore::PowerDecision::CLOCK_REQUIRED);
 assert(FeatureCore::powerDecision(false,0,true,true,100,0,true,true)==FeatureCore::PowerDecision::SLEEP);
 assert(FeatureCore::powerDecision(false,0,true,true,100,0,true,false)==FeatureCore::PowerDecision::DRAINING);
 assert(FeatureCore::powerDecision(true,4000,false,false,0,0,true,true)==FeatureCore::PowerDecision::RESTART);
 assert(FeatureCore::powerDecision(true,1000,false,false,0,0,true,true)==FeatureCore::PowerDecision::DRAINING);
 assert(FeatureCore::reconnectDelay(99)==30000);std::cout<<"PASS production schedule/network/radio framing and simulated hardware boundary tests\n";
}
