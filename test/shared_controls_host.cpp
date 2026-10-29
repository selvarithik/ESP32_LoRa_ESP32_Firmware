#include "../src/config/transport_trial_core.h"
#include "../src/communication/e220_core.h"
#include "../src/core/feature_core.h"
#include "../src/config/hardware_profile.h"
#include <cassert>
#include <iostream>
int main(){assert(FeatureCore::manualSleepRemaining(100,100,60)==65);assert(FeatureCore::manualSleepRemaining(65100,100,60)==0);assert(FeatureCore::manualSleepRemaining(1000,0xfffffff0,60)==64);
 using namespace TransportTrial;
 assert(id("TX-test_1")&&!id("")&&!id("TX/bad"));
 assert(!expired(Phase::PREPARED,100,700));assert(expired(Phase::PREPARED,700,700));
 assert(!canConfirm(Phase::PREPARED,true,true,true,100,200));
 assert(!canConfirm(Phase::TRIAL,false,true,true,100,200));
 assert(!canConfirm(Phase::TRIAL,true,false,true,100,200));
 assert(!canConfirm(Phase::TRIAL,true,true,false,100,200));
 assert(!canConfirm(Phase::TRIAL,true,true,true,200,200));
 assert(canConfirm(Phase::TRIAL,true,true,true,199,200));
 assert(HardwareProfile::roleMatches(true)&&HardwareProfile::roleMatches(false));
 assert(HardwareProfile::inaAllowed(true)&&!HardwareProfile::inaAllowed(false));
 assert(E220Core::mode("NORMAL")==0&&E220Core::mode("WOR_TX")==1&&E220Core::mode("WOR_RX")==2);
 const uint8_t previous[6]={0,0,0x62,0x3c,23,0x28};uint8_t out[6],reply[9]={0xc1,0,6};
 E220Core::Settings s;s.address=0x1234;s.baud=9600;s.airRate=2;s.channel=17;s.power=17;s.packet=128;s.fixed=true;s.rssi=true;s.lbt=true;
 for(unsigned ms=500;ms<=4000;ms+=500){s.worMs=ms;assert(E220Core::encodeSettings(s,previous,out));memcpy(reply+3,out,6);E220Core::Settings read;assert(E220Core::settings(reply,9,read));assert(read.address==s.address&&read.channel==s.channel&&read.worMs==ms&&read.power==17&&read.packet==128&&read.fixed&&read.rssi&&read.lbt);assert((out[3]&0x3c)==0x3c&&(out[5]&0x28)==0x28);}
 s.channel=81;assert(!E220Core::encodeSettings(s,previous,out));s.channel=17;s.worMs=501;assert(!E220Core::encodeSettings(s,previous,out));s.worMs=2000;s.power=30;assert(!E220Core::encodeSettings(s,previous,out));
 assert(FeatureCore::powerDecision(false,0,true,true,60,1,true,true)==FeatureCore::PowerDecision::DRAINING);
 assert(FeatureCore::powerDecision(false,0,true,true,60,0,true,false)==FeatureCore::PowerDecision::DRAINING);
 assert(FeatureCore::powerDecision(false,0,true,true,60,0,true,true)==FeatureCore::PowerDecision::SLEEP);
 std::cout<<"PASS production transport proof/deadline policy, shared roles, E220 register encoding/readback and sleep drain decisions\n";
}
