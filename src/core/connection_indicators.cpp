#include "connection_indicators.h"
#include "indicator_core.h"
#include "config_manager.h"
#include "../communication/transport_manager.h"
#include "../communication/wifi_manager.h"
#include <ArduinoJson.h>
#include <TM1637Display.h>
#include <memory>
#include <new>
ConnectionIndicators connectionIndicators;
namespace {
constexpr int LedPin=2,ClockPin=18,DataPin=19;
class CheckedDisplay:public TM1637Display {
public:
 CheckedDisplay():TM1637Display(ClockPin,DataPin){}
 bool render(const uint8_t* segments){bool ok=true;start();ok=!writeByte(0x40)&&ok;stop();start();ok=!writeByte(0xc0)&&ok;for(unsigned i=0;i<4;++i)ok=!writeByte(segments[i])&&ok;stop();start();ok=!writeByte(0x8c)&&ok;stop();return ok;}
};
std::unique_ptr<CheckedDisplay> display;
bool displayAck=false,idleClock=false,idleData=false;uint32_t ackFailures=0,connectedAt=0,connections=0,startedAt=0;
bool enabled=false,activeHigh=true,server=false,started=false,lastWifi=false,displayRequested=false;
uint32_t checked=0,drawn=0,waitingAt=0,receipt=0;IndicatorCore::Frame frame;
}
void ConnectionIndicators::begin(){auto& c=configManager.get();enabled=c.server_led_enabled;activeHigh=c.server_led_active_high;
 if(enabled){digitalWrite(LedPin,activeHigh?LOW:HIGH);pinMode(LedPin,OUTPUT);}
 displayRequested=c.role==DeviceRole::MASTER&&c.tm1637_enabled;
 if(displayRequested){display.reset(new(std::nothrow) CheckedDisplay());}
 started=true;startedAt=millis();waitingAt=millis();checked=millis()-250;loop();
}
void ConnectionIndicators::loop(){if(!started)return;uint32_t now=millis();if((uint32_t)(now-checked)<250)return;checked=now;
 bool wifi=wifiManager.isConnected();receipt=transportManager.lastGatewayReceipt();bool up=IndicatorCore::connected(wifi,now,receipt);bool changed=up!=server||wifi!=lastWifi;
 if(!server&&up){connectedAt=now;if(connections<0xffffffff)++connections;}if(server&&!up)waitingAt=now;if(!lastWifi&&wifi&&!up)waitingAt=now;server=up;lastWifi=wifi;
 if(enabled)digitalWrite(LedPin,(server==activeHigh)?HIGH:LOW);
 if(display&&(changed||!drawn||(uint32_t)(now-drawn)>=1000)){idleClock=digitalRead(ClockPin);idleData=digitalRead(DataPin);
  if((uint32_t)(now-startedAt)<2000){for(int i=0;i<4;++i)frame.segments[i]=0x7f;snprintf(frame.text,6,"8888");}else frame=IndicatorCore::screen(wifi,server,now,receipt,waitingAt,wifi?wifiManager.rssi():0,connectedAt);
  displayAck=display->render(frame.segments)&&idleClock&&idleData;if(!displayAck)++ackFailures;drawn=now;}
}
void ConnectionIndicators::stop(){if(enabled)digitalWrite(LedPin,activeHigh?LOW:HIGH);if(display){uint8_t blank[4]={0};display->render(blank);}server=false;started=false;}
String ConnectionIndicators::statusJson()const{JsonDocument d;d["server_connected"]=server;d["basis"]="AUTHENTICATED_GATEWAY_RECEIPT";d["receipt_timeout_seconds"]=15;d["led_enabled"]=enabled;d["led_gpio"]=LedPin;d["led_on_requested"]=enabled&&server;d["led_active_high"]=activeHigh;
 d["display_acknowledged"]=displayAck;d["display_ack_failures"]=ackFailures;d["display_idle_clk_high"]=idleClock;d["display_idle_dio_high"]=idleData;d["connection_count_since_boot"]=connections;d["connected_duration_seconds"]=server?(uint32_t)(millis()-connectedAt)/1000:0;d["display_enabled"]=bool(display);d["display_state"]=!displayRequested?"DISABLED_OR_NODE":!display?"ALLOCATION_FAILED":!idleClock||!idleData?"LINE_NOT_HIGH":displayAck?"ACKNOWLEDGED_VISUAL_UNVERIFIED":"NO_ACK";d["display_clk_gpio"]=ClockPin;d["display_dio_gpio"]=DataPin;d["display_text"]=display?frame.text:"";d["physical_visual_verified"]=false;
 if(receipt)d["gateway_ack_age_seconds"]=(uint32_t)(millis()-receipt)/1000;else d["gateway_ack_age_seconds"]=nullptr;String raw;serializeJson(d,raw);return raw;
}
