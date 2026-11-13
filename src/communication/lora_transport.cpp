#include "lora_transport.h"
#include "e220_core.h"
#include "radio_turn.h"
#include "radio_airtime.h"
#include "../config/hardware_profile.h"
#include "../core/logger.h"
#include <esp_mac.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <memory>
#include <new>
#include <driver/gpio.h>
LoRaTransport loraTransport;
namespace {
HardwareSerial normalUart(2),diagnosticUart(1);bool useDiagnosticUart=false;
HardwareSerial& radioUart(){return useDiagnosticUart?diagnosticUart:normalUart;}
DeviceConfig config;
enum class State:uint8_t {Off,WaitIdle,WaitMode,Reading,Writing,Restoring,Ready,SleepSwitch,Sleeping,Fault};
State state=State::Off;const char* error="DISABLED";bool initialized=false,probeOnly=false,restoreHigh0=false,restoreHigh1=false;
bool probePinsConfigured=false,settingsQuerySent=false;const char* probePhase="NOT_STARTED";
uint32_t since=0,auxSince=0,nextTx=0,txStarted=0;bool auxHigh=false;
E220Core::Settings observed;bool observedValid=false;uint8_t desiredBytes[6]{};bool wrote=false;unsigned registerWrites=0;int lastRssi=0;bool hasRssi=false,txAwake=false;uint32_t lastRfRxAt=0;uint32_t txAwakeAt=0;
uint8_t settingsBytes[9]{};unsigned settingsLength=0;E220Core::SettingsReader settingsReader;unsigned settingsAttempts=0;
uint8_t ownMac[6]{},target[6]{};uint32_t transfer=0,rxCount=0,txCount=0,rejected=0,timeouts=0,authFailures=0,completed=0;
E220Core::Assembler assemblies;
uint8_t encoded[202]{},raw[200]{};
E220Core::ReceiveStream rxStream;
uint8_t txData[E220Core::MaxMessage]{};size_t txLength=0;uint8_t txPart=0;String txId,lastSender,resultId;bool resultOk=false,hasResult=false,rxReady=false;
Message received;
RadioTurn::Exchange radioTurn;
RadioAirtime::Budget airtime;
uint32_t bootTxAt=0,nextPollAt=0,budgetDenials=0,turnToken=0;
uint8_t slotFragments=0;
bool bootQuiet=true,pollCooling=false,replyPaused=false,budgetBlocked=false,partialReply=false;
uint32_t masterQuietUntil=0;
bool masterWaiting=false,masterQuiet=true;
struct Seen {uint8_t sender[6]{};uint32_t transfer=0,crc=0,at=0;bool used=false;};Seen seen[32];uint8_t seenNext=0;
bool mac(const String& id,uint8_t* out){if(id=="BROADCAST"){memset(out,255,6);return true;}if(id.length()==18&&id.startsWith("ESP32-")){for(int i=0;i<6;++i){String piece=id.substring(6+(5-i)*2,8+(5-i)*2);char* end=nullptr;unsigned n=strtoul(piece.c_str(),&end,16);if(!end||*end)return false;out[i]=n;}return true;}unsigned values[6];char extra;if(sscanf(id.c_str(),"%2x:%2x:%2x:%2x:%2x:%2x%c",&values[0],&values[1],&values[2],&values[3],&values[4],&values[5],&extra)!=6||id.length()!=17)return false;for(int i=0;i<6;++i)out[i]=values[i];return true;}
String identity(const uint8_t* bytes){char value[19];snprintf(value,sizeof(value),"ESP32-%02X%02X%02X%02X%02X%02X",bytes[5],bytes[4],bytes[3],bytes[2],bytes[1],bytes[0]);return value;}
bool tag(const uint8_t* data,size_t n,uint8_t* output){return !mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),(const unsigned char*)config.mesh_key.c_str(),config.mesh_key.length(),data,n,output);}
bool matches(){return observed.baud==config.lora_baud&&observed.address==(uint16_t(config.lora_address_high)<<8|config.lora_address_low)&&observed.channel==config.lora_channel&&observed.airRate==config.lora_air_rate&&observed.power==config.lora_tx_power&&observed.fixed==config.lora_fixed_mode&&observed.rssi==config.lora_rssi&&observed.lbt==config.lora_lbt&&(!config.lora_apply_settings||(observed.packet==config.lora_packet_bytes&&observed.worMs==config.lora_wor_cycle_ms))&&(config.lora_work_mode=="NORMAL"||observed.worMs==config.lora_wor_cycle_ms);}
void restore(const char* reason){error=reason;digitalWrite(config.lora_m0_pin,restoreHigh0);digitalWrite(config.lora_m1_pin,restoreHigh1);radioUart().updateBaudRate(config.lora_baud);state=State::Restoring;since=millis();initialized=false;}
size_t pack(const Message& m,uint8_t* out){JsonDocument payload,doc;if(deserializeJson(payload,m.payload)||!payload.is<JsonObject>())return 0;auto a=doc.to<JsonArray>();a.add(m.protocolVersion);a.add(m.typeString());a.add(m.ttl);a.add(m.timestamp);a.add(m.sequence);a.add(m.messageId);a.add(m.source);a.add(m.destination);a.add(m.nextHop);a.add(m.origin.isEmpty()?m.source:m.origin);a.add(m.correlationId);a.add(payload.as<JsonVariant>());a.add(m.radioHopCount+1);a.add(m.radioMaxHops?m.radioMaxHops:m.ttl);size_t n=measureMsgPack(doc);return n<=E220Core::MaxMessage?serializeMsgPack(doc,out,E220Core::MaxMessage):0;}
bool unpack(const uint8_t* bytes,size_t n,Message& m){JsonDocument d;if(deserializeMsgPack(d,bytes,n)||!d.is<JsonArray>()||d.size()!=14)return false;auto a=d.as<JsonArray>();if(!a[0].is<uint8_t>()||!a[1].is<const char*>()||!a[2].is<uint8_t>()||!a[3].is<uint32_t>()||!a[4].is<uint32_t>()||!a[11].is<JsonObject>()||!a[12].is<uint8_t>()||!a[13].is<uint8_t>()||a[12].as<unsigned>()<1||a[13].as<unsigned>()>15||a[12].as<unsigned>()>a[13].as<unsigned>())return false;for(unsigned i=5;i<11;++i)if(!a[i].is<const char*>()||a[i].as<String>().length()>48)return false;Message p;p.protocolVersion=a[0];p.type=packetTypeFromString(a[1]);p.ttl=a[2];p.timestamp=a[3];p.sequence=a[4];p.messageId=a[5].as<String>();p.source=a[6].as<String>();p.destination=a[7].as<String>();p.nextHop=a[8].as<String>();p.origin=a[9].as<String>();p.correlationId=a[10].as<String>();serializeJson(a[11],p.payload);if(!Message::fromJson(p.toJson(),m))return false;m.radioHopCount=a[12];m.radioMaxHops=a[13];return true;}
bool isMaster(){return config.role==DeviceRole::MASTER;}
bool pollType(PacketType type){
 return type==PacketType::HELLO||type==PacketType::ACK||type==PacketType::COMMAND||
  type==PacketType::CONFIG_GET||type==PacketType::CONFIG_UPDATE;
}
bool validPoll(const Message& m,const String& sender){
 if(!pollType(m.type))return false;
 JsonDocument body;if(deserializeJson(body,m.payload))return false;
 if(m.type==PacketType::HELLO)return m.source==sender&&body["device_id"].as<String>()==sender&&body["role"]=="MASTER";
 if(m.type==PacketType::ACK)return m.source==sender&&!m.correlationId.isEmpty()&&body["received_message_id"].as<String>()==m.correlationId;
 return (m.source==sender||m.source=="GATEWAY")&&body["command"].is<const char*>()&&body["arguments"].is<JsonObject>()&&
  body["command_id"].as<String>()==m.messageId;
}
void finishSend(bool ok){txLength=0;replyPaused=false;hasResult=true;resultId=txId;resultOk=ok;}
void receiveFrame(const uint8_t* encoded,size_t length){size_t n=E220Core::cobsDecode(encoded,length,raw,sizeof(raw));if(n<=E220Core::Header+E220Core::Tag){++rejected;return;}uint8_t signature[32];size_t body=n-E220Core::Tag;if(!tag(raw,body,signature)){++authFailures;return;}uint8_t diff=0;for(unsigned i=0;i<E220Core::Tag;++i)diff|=signature[i]^raw[body+i];if(diff){++authFailures;return;}E220Core::Fragment f;if(!E220Core::parse(raw,body,f)){++rejected;return;}
 if(memcmp(f.target,ownMac,6)||!memcmp(f.sender,ownMac,6))return;
 const uint32_t now=millis();
 const String sender=identity(f.sender);
 if(isMaster()){
  if(!masterWaiting||!radioTurn.matches(f.sender,f.turn,now))return;
 }else if(f.turn!=f.transfer||config.master_id!=sender||(radioTurn.busy()&&!radioTurn.expired(now)))return;
 for(auto& item:seen)if(item.used&&millis()-item.at<E220Core::AssemblyLifetime&&item.transfer==f.transfer&&!memcmp(item.sender,f.sender,6)){return;}
 const uint32_t rejectedBefore=assemblies.rejected;
 auto* message=assemblies.add(f,now,isMaster()?RadioAirtime::ReplyAssemblyMs:E220Core::AssemblyLifetime);
 if(!message){
  if(isMaster()&&f.count>1&&assemblies.rejected==rejectedBefore)partialReply=true;
  return;
 }
 Message parsed;
 const bool valid=!rxReady&&unpack(message->data,message->total,parsed)&&parsed.radioHopCount==1&&parsed.nextHop==config.device_id&&
  (isMaster()?parsed.source==sender&&(parsed.destination==config.device_id||parsed.destination=="GATEWAY"):
   parsed.destination==config.device_id&&validPoll(parsed,sender));
 if(valid){
  if(isMaster()){
   radioTurn.replied();
   masterWaiting=false;
   radioTurn.clear();
   masterQuiet=true;
   masterQuietUntil=now+RadioTurn::TurnaroundMs;
   error="NONE";
   finishSend(true);
  }else radioTurn.open(f.sender,f.transfer,now+RadioTurn::TurnaroundMs,now+RadioTurn::ReplyMs-RadioTurn::ReplyGuardMs);
  received=parsed;lastSender=sender;rxReady=true;lastRfRxAt=now;hasRssi=false;++rxCount;
  auto& item=seen[seenNext++%32];item.used=true;item.transfer=f.transfer;item.crc=f.crc;item.at=now;memcpy(item.sender,f.sender,6);
 }else ++rejected;
 message->used=false;
}
}
bool LoRaTransport::begin(const DeviceConfig& c){config=c;state=State::Off;error="DISABLED";initialized=false;bootTxAt=millis()+RadioAirtime::BootQuietMs;masterQuietUntil=millis()+RadioTurn::ReplyMs;esp_read_mac(ownMac,ESP_MAC_WIFI_STA);if(!HardwareProfile::roleMatches(c.role==DeviceRole::MASTER)){error="BUILD_ROLE_MISMATCH";return false;}uint8_t configuredMac[6];if(!mac(c.device_id,configuredMac)||memcmp(configuredMac,ownMac,6)){error="IDENTITY_NOT_HARDWARE_MAC";return false;}gpio_deep_sleep_hold_dis();gpio_hold_dis((gpio_num_t)c.lora_m0_pin);gpio_hold_dis((gpio_num_t)c.lora_m1_pin);if(!c.lora_enabled)return false;probeOnly=false;return requestProbe(false);}
bool LoRaTransport::requestProbe(bool readOnly,uint8_t diagnosticPort){
 if((diagnosticPort!=1&&diagnosticPort!=2)||(!readOnly&&diagnosticPort!=2)){error="DIAGNOSTIC_UART_INVALID";return false;}if(!HardwareProfile::roleMatches(config.role==DeviceRole::MASTER)){error="BUILD_ROLE_MISMATCH";return false;}uint8_t configuredMac[6];if(!mac(config.device_id,configuredMac)||memcmp(configuredMac,ownMac,6)){error="IDENTITY_NOT_HARDWARE_MAC";return false;}if(state!=State::Off&&state!=State::Fault)return false;probeOnly=readOnly||!config.lora_enabled;if(config.lora_uart!=2||!HardwareProfile::radioUartPins(config.role==DeviceRole::MASTER,config.lora_rx_pin,config.lora_tx_pin)||config.lora_aux_pin!=HardwareProfile::RadioAux||config.lora_m0_pin!=HardwareProfile::RadioM0||config.lora_m1_pin!=HardwareProfile::RadioM1){error="UNVERIFIED_RADIO_PINS";return false;}if(!probeOnly&&config.mesh_key.length()<32){error="MESH_CREDENTIAL_REQUIRED";return false;}
 normalUart.end();diagnosticUart.end();useDiagnosticUart=diagnosticPort==1;
 pinMode(config.lora_aux_pin,INPUT);pinMode(config.lora_m0_pin,INPUT);pinMode(config.lora_m1_pin,INPUT);restoreHigh0=digitalRead(config.lora_m0_pin);restoreHigh1=digitalRead(config.lora_m1_pin);digitalWrite(config.lora_m0_pin,restoreHigh0);digitalWrite(config.lora_m1_pin,restoreHigh1);pinMode(config.lora_m0_pin,OUTPUT);pinMode(config.lora_m1_pin,OUTPUT);probePinsConfigured=true;settingsQuerySent=false;settingsLength=0;settingsAttempts=0;settingsReader.reset();probePhase="WAIT_IDLE_AUX";radioUart().end();radioUart().setRxBufferSize(512);radioUart().setTxBufferSize(256);radioUart().begin(9600,SERIAL_8N1,config.lora_rx_pin,config.lora_tx_pin);gpio_pullup_en((gpio_num_t)config.lora_rx_pin);radioUart().setRxTimeout(2);radioUart().setRxFIFOFull(1);wrote=false;observedValid=false;observed=E220Core::Settings{};state=State::WaitIdle;since=millis();auxSince=since;auxHigh=false;error="INSPECTING";return true;}
void LoRaTransport::loop(){uint32_t now=millis();if(state==State::Off||state==State::Fault)return;bool high=auxReady();if(high&&!auxHigh)auxSince=now;auxHigh=high;
 if(state==State::Sleeping)return;
 if(state==State::SleepSwitch){if(high&&now-since>=5&&now-auxSince>=2)state=State::Sleeping;else if(now-since>3000){state=State::Fault;error="SLEEP_MODE_TIMEOUT";}return;}
 if(state!=State::Ready&&state!=State::Restoring&&now-since>3000){
  if(state==State::Reading&&!wrote&&settingsAttempts<3){state=State::WaitMode;probePhase="RETRY_CONFIG_READ";since=now;auxSince=now;return;}
  ++timeouts;restore(state==State::Reading&&settingsReader.total?(settingsReader.total<9?"SETTINGS_PARTIAL_RESPONSE":"INVALID_PUBLIC_SETTINGS"):"SETTINGS_READ_TIMEOUT");return;
 }
 if(state==State::WaitIdle&&high&&now-auxSince>=2){digitalWrite(config.lora_m0_pin,HIGH);digitalWrite(config.lora_m1_pin,HIGH);state=State::WaitMode;probePhase="WAIT_CONFIG_AUX";since=now;auxSince=now;return;}
 if(state==State::WaitMode&&high&&now-since>=100&&now-auxSince>=20){for(unsigned drain=0;drain<512&&radioUart().available();++drain)radioUart().read();const uint8_t query[]={0xc1,0,6};if(radioUart().availableForWrite()<3)return;if(radioUart().write(query,3)!=3){restore("SETTINGS_QUERY_WRITE_FAILED");return;}++settingsAttempts;settingsReader.reset();settingsQuerySent=true;settingsLength=0;probePhase="READ_PUBLIC_REGISTERS";state=State::Reading;since=now;return;}
 if(state==State::Writing){for(unsigned n=0;n<32&&radioUart().available()&&settingsLength<9;++n)settingsBytes[settingsLength++]=radioUart().read();if(settingsLength<9)return;if(settingsBytes[0]!=0xc1||settingsBytes[1]!=0||settingsBytes[2]!=6||memcmp(settingsBytes+3,desiredBytes,6)){restore("REGISTER_WRITE_REJECTED");return;}observedValid=false;
  settingsReader.reset();settingsLength=0;
  // Wait for AUX and issue a separate read after the persistent-write echo.
  state=State::WaitMode;probePhase="WAIT_POST_WRITE_AUX";since=now;auxSince=now;return;}
 if(state==State::Reading){
  for(unsigned budget=0;budget<32&&radioUart().available();++budget){if(settingsReader.add(radioUart().read(),observed)){observedValid=true;break;}}
  settingsLength=settingsReader.length;memcpy(settingsBytes,settingsReader.bytes,settingsLength);
  if(!observedValid)return;probePhase="PUBLIC_REGISTERS_READ";if(probeOnly){restore("PROBE_COMPLETE_NO_RF");return;}if(!matches()){
 if(!config.lora_apply_settings||wrote){restore("SETTINGS_MISMATCH_NO_RF");return;}
 auto desired=observed;desired.address=(uint16_t(config.lora_address_high)<<8)|config.lora_address_low;desired.baud=config.lora_baud;desired.channel=config.lora_channel;desired.airRate=config.lora_air_rate;desired.power=config.lora_tx_power;desired.packet=config.lora_packet_bytes;desired.fixed=config.lora_fixed_mode;desired.rssi=config.lora_rssi;desired.lbt=config.lora_lbt;desired.worMs=config.lora_wor_cycle_ms;
 if(!E220Core::encodeSettings(desired,settingsBytes+3,desiredBytes)){restore("INVALID_REGISTER_CONFIGURATION");return;}uint8_t write[9]={0xc0,0,6};memcpy(write+3,desiredBytes,6);if(radioUart().availableForWrite()<9)return;radioUart().write(write,9);wrote=true;++registerWrites;settingsLength=0;state=State::Writing;since=now;return;}
if(!E220Core::span(observed.packet)){restore("SUBPACKET_TOO_SMALL_NO_RF");return;}unsigned mode=E220Core::mode(config.lora_work_mode.c_str());digitalWrite(config.lora_m0_pin,mode&1);digitalWrite(config.lora_m1_pin,(mode>>1)&1);radioUart().updateBaudRate(observed.baud);state=State::Restoring;since=now;error="STARTING_NORMAL_MODE";return;}
 if(state==State::Restoring){if(high&&now-since>=5&&now-auxSince>=2){if(!strcmp(error,"STARTING_NORMAL_MODE")){state=State::Ready;initialized=true;error="NONE";logger.info("LORA","Read-only settings check passed; normal mode ready (hardware link unverified)");}else{state=State::Fault;radioUart().end();}}else if(now-since>3000){state=State::Fault;radioUart().end();error="MODE_RESTORE_TIMEOUT";++timeouts;}return;}
 if(state!=State::Ready)return;
 if(bootQuiet&&RadioTurn::due(now,bootTxAt))bootQuiet=false;
 if(pollCooling&&RadioTurn::due(now,nextPollAt))pollCooling=false;
 const uint32_t used=airtime.used(now);
 if(budgetBlocked&&used<=RadioAirtime::LimitUs-RadioAirtime::WorstFragmentUs)budgetBlocked=false;
 if(masterQuiet&&RadioTurn::due(now,masterQuietUntil))masterQuiet=false;
 if(isMaster()&&masterWaiting&&radioTurn.expired(now)){
  radioTurn.clear();
  masterWaiting=false;
  masterQuiet=true;
  masterQuietUntil=RadioTurn::retryAt(now,esp_random());
  if(!partialReply){++timeouts;error="RADIO_REPLY_TIMEOUT";}
  finishSend(partialReply);
 }
 if(txAwake&&config.lora_work_mode=="WOR_RX"&&idle()&&now-txAwakeAt>3000&&high&&now-auxSince>=2){digitalWrite(config.lora_m0_pin,LOW);digitalWrite(config.lora_m1_pin,HIGH);txAwake=false;state=State::Restoring;since=now;error="STARTING_NORMAL_MODE";return;}
 assemblies.expire(now,isMaster()?RadioAirtime::ReplyAssemblyMs:E220Core::AssemblyLifetime);
 const auto signal=[](int rssi){lastRssi=rssi;hasRssi=true;};
 if(rxStream.expire(now,signal))++rejected;
 for(unsigned budget=0;budget<256&&radioUart().available();++budget){
  if(!rxStream.add(radioUart().read(),now,observed.rssi,
   [](const uint8_t* bytes,size_t length){
    const uint32_t before=rxCount;
    receiveFrame(bytes,length);
    return rxCount!=before;
   },
   signal))++rejected;
 }
 if(replyPaused&&!bootQuiet&&!budgetBlocked&&radioTurn.canClaim(target,now)){
  turnToken=radioTurn.claim();slotFragments=0;replyPaused=false;
 }
 if(txLength&&now-txStarted>(isMaster()?config.message_max_age_ms:RadioAirtime::ReplyAssemblyMs)){
  finishSend(false);error=high?"MESSAGE_TX_EXPIRED":"AUX_BUSY_TIMEOUT";++timeouts;
 }
 if(!txLength||replyPaused||!high||now-auxSince<2||(int32_t)(now-nextTx)<0)return;
 uint8_t step=E220Core::span(observed.packet);size_t n=E220Core::fragment(txData,txLength,transfer,ownMac,target,step,txPart,raw,turnToken);uint8_t signature[32];if(!n||!tag(raw,n,signature)){finishSend(false);return;}memcpy(raw+n,signature,E220Core::Tag);n=E220Core::cobsEncode(raw,n+E220Core::Tag,encoded,sizeof(encoded));if(!n||n>observed.packet){finishSend(false);++rejected;return;}
 const size_t prefix=observed.fixed?3:0;
 const uint32_t guard=(RadioAirtime::fragmentUs(n+prefix)+999)/1000+
  uint32_t(n+prefix)*12000/observed.baud+50+
  (observed.lbt?2000:0);
 if(!isMaster()&&!radioTurn.canWrite(now,guard)){replyPaused=true;return;}
 if(radioUart().availableForWrite()<int(n+prefix))return;
 if(!airtime.debit(now,n+prefix,guard)){
  ++budgetDenials;budgetBlocked=true;
  logger.warn("LORA","Airtime budget exhausted; fragment not transmitted");
  if(isMaster())finishSend(false);else replyPaused=true;
  return;
 }
 nextTx=now+guard;
 if(isMaster()&&!slotFragments){pollCooling=true;nextPollAt=now+RadioAirtime::PollIntervalMs;}
 const uint8_t address[]={255,255,observed.channel};
 if((prefix&&radioUart().write(address,prefix)!=prefix)||radioUart().write(encoded,n)!=n){
  ++timeouts;
  if(isMaster()){masterQuiet=true;masterQuietUntil=nextTx+RadioTurn::ReplyMs;}
  finishSend(false);
  return;
 }
 ++txCount;++txPart;++slotFragments;
 if(size_t(txPart)*step>=txLength){
  ++completed;
  if(isMaster()){
   txLength=0;
   radioTurn.open(target,transfer,now,nextTx+RadioTurn::ReplyMs);
   masterWaiting=true;partialReply=false;
  }else finishSend(true);
 }else if(!isMaster()&&!RadioAirtime::slotAllows(slotFragments,RadioAirtime::MaxReplyFragments))replyPaused=true;
}
bool LoRaTransport::send(const Message& m){
 const uint32_t now=millis();
 const String hop=m.nextHop.isEmpty()?m.destination:m.nextHop;
 uint8_t address[6];
 if(bootQuiet||budgetBlocked||config.lora_work_mode=="WOR_TX"||hop=="BROADCAST"||m.radioHopCount||!mac(hop,address)||!memcmp(address,ownMac,6))return false;
 if(isMaster()){
  if(masterWaiting||masterQuiet||pollCooling||m.destination!=hop||!pollType(m.type))return false;
 }else if(!radioTurn.canClaim(address,now)||m.source!=config.device_id||
   (m.destination!=hop&&m.destination!="GATEWAY"))return false;
 if(config.lora_work_mode=="WOR_RX"&&!txAwake){if(state==State::Ready&&idle()&&auxReady()){digitalWrite(config.lora_m0_pin,LOW);digitalWrite(config.lora_m1_pin,LOW);txAwake=true;txAwakeAt=now;state=State::Restoring;since=now;error="STARTING_NORMAL_MODE";}return false;}
 if(!initialized||state!=State::Ready||txLength||hasResult||!RadioTurn::due(now,nextTx)||m.payload.length()>MAX_PAYLOAD_BYTES)return false;
 Message checked;if(!Message::fromJson(m.toJson(),checked))return false;
 txLength=pack(m,txData);if(!txLength)return false;
 const uint8_t step=E220Core::span(observed.packet);
 if((txLength+step-1)/step>255){txLength=0;return false;}
 memcpy(target,address,sizeof(target));
 txId=m.messageId;
 if(isMaster()&&(txLength+step-1)/step>RadioAirtime::MaxPollFragments){
  logger.warn("LORA","Poll exceeds the fragment limit");finishSend(false);return true;
 }
 txPart=slotFragments=0;transfer=esp_random();turnToken=isMaster()?transfer:radioTurn.claim();txStarted=now;txAwakeAt=now;
 return true;
}
bool LoRaTransport::canReply()const{
 uint8_t parent[6];
 return !bootQuiet&&!budgetBlocked&&config.lora_work_mode!="WOR_TX"&&!isMaster()&&initialized&&state==State::Ready&&!txLength&&!hasResult&&mac(config.master_id,parent)&&radioTurn.canClaim(parent,millis());
}
bool LoRaTransport::canPoll()const{
 return isMaster()&&initialized&&state==State::Ready&&config.lora_work_mode!="WOR_TX"&&!bootQuiet&&!pollCooling&&!budgetBlocked&&!masterQuiet&&!masterWaiting&&!txLength&&!hasResult;
}
uint32_t LoRaTransport::airtimeRefusals()const{return budgetDenials;}
bool LoRaTransport::powerSleep(bool sleep){
 if(!config.lora_enabled)return true;
 if(sleep){if(state==State::Sleeping)return true;if(state!=State::Ready||!idle()||!auxReady())return false;digitalWrite(config.lora_m0_pin,HIGH);digitalWrite(config.lora_m1_pin,HIGH);state=State::SleepSwitch;since=millis();return false;}
 if(state==State::Sleeping||state==State::SleepSwitch){txAwake=false;unsigned mode=E220Core::mode(config.lora_work_mode.c_str());digitalWrite(config.lora_m0_pin,mode&1);digitalWrite(config.lora_m1_pin,(mode>>1)&1);state=State::Restoring;since=millis();error="STARTING_NORMAL_MODE";}return state==State::Ready;
}
bool LoRaTransport::available(){return rxReady;}
String LoRaTransport::isolatedUartLoopback(){
 JsonDocument out;out["ok"]=false;
 // Physical USB confirmation is checked by commissioning. Never interrupt an active radio.
 if(config.lora_enabled||(state!=State::Off&&state!=State::Fault)||txLength||rxReady||hasResult){out["code"]="RADIO_MUST_BE_DISABLED_AND_IDLE";}
 else if(config.lora_uart!=2||!HardwareProfile::radioUartPins(config.role==DeviceRole::MASTER,config.lora_rx_pin,config.lora_tx_pin)){out["code"]="UNVERIFIED_RADIO_PINS";}
 else {
  normalUart.end();diagnosticUart.end();useDiagnosticUart=false;
  const uint8_t expected[]={0x55,0xaa,0x00,0xff,0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21,0x00,0xaa,0x55,0xff};
  uint8_t received[sizeof(expected)+1]{};size_t count=0;
  radioUart().end();radioUart().setRxBufferSize(512);radioUart().setTxBufferSize(256);radioUart().begin(9600,SERIAL_8N1,config.lora_rx_pin,config.lora_tx_pin);
  gpio_pullup_en((gpio_num_t)config.lora_rx_pin);delay(3);
  for(unsigned n=0;n<512&&radioUart().available();++n)radioUart().read();
  uint32_t started=millis();size_t sent=radioUart().write(expected,sizeof(expected));
  while(millis()-started<250){while(radioUart().available()&&count<sizeof(received))received[count++]=radioUart().read();delay(1);}
  bool ok=sent==sizeof(expected)&&count==sizeof(expected)&&memcmp(expected,received,sizeof(expected))==0;
  out["ok"]=ok;out["code"]=ok?"UART_LOOPBACK_PASS_RADIO_NOT_TESTED":sent!=sizeof(expected)?"UART_TX_WRITE_FAILED":!count?"UART_RX_NO_RESPONSE":count!=sizeof(expected)?"UART_RX_LENGTH_MISMATCH":"UART_RX_DATA_MISMATCH";
  out["sent_bytes"]=sent;out["received_bytes"]=count;out["elapsed_ms"]=millis()-started;out["rx_gpio"]=config.lora_rx_pin;out["tx_gpio"]=config.lora_tx_pin;out["baud"]=9600;
  // Release UART pins. The next normal read-only probe reinitializes the peripheral.
  radioUart().end();pinMode(config.lora_rx_pin,INPUT);pinMode(config.lora_tx_pin,INPUT);initialized=false;state=State::Off;error="DISABLED";
 }
 String result;serializeJson(out,result);return result;
}
bool LoRaTransport::receive(Message& m){if(!rxReady)return false;m=received;rxReady=false;return true;}
bool LoRaTransport::auxReady()const{return state!=State::Off&&state!=State::Fault&&digitalRead(config.lora_aux_pin)==HIGH;}
bool LoRaTransport::isInitialized()const{return initialized;}
bool LoRaTransport::hasFreshReception()const{
 return E220Core::freshReception(initialized&&state==State::Ready,rxCount!=0,millis(),lastRfRxAt,config.route_timeout_seconds*1000UL);
}
bool LoRaTransport::idle()const{return !txLength&&!rxReady&&!hasResult&&!masterWaiting&&RadioTurn::due(millis(),nextTx);}
String LoRaTransport::sender()const{return lastSender;}
bool LoRaTransport::delivery(String& id,bool& ok){if(!hasResult)return false;id=resultId;ok=resultOk;hasResult=false;return true;}
String LoRaTransport::statusJson()const{JsonDocument d;d["state"]=state==State::Sleeping?"SLEEPING":initialized?"READY_LINK_UNVERIFIED":state==State::Off?"DISABLED":state==State::Fault?"ERROR":"INSPECTING";d["error"]=error;d["module"]="E220-900T22D";const bool fresh=hasFreshReception();d["hardware_link_verified"]=fresh;
const uint32_t rxAge=millis()-lastRfRxAt;
d["rf_receive_state"]=!initialized||state!=State::Ready?"INACTIVE":!rxCount?"WAITING_FOR_PEER":fresh?"RECEIVING":"STALE";
if(rxCount){d["last_rf_peer"]=lastSender;d["last_rf_age_ms"]=rxAge;}
d["public_settings_read"]=observedValid;d["register_writes"]=registerWrites;d["work_mode"]=config.lora_work_mode;d["sleeping"]=state==State::Sleeping;d["wor_cycle_ms"]=config.lora_wor_cycle_ms;d["aux_ready"]=auxReady();d["probe_phase"]=probePhase;d["settings_query_sent"]=settingsQuerySent;d["settings_response_bytes"]=settingsReader.total;d["settings_read_attempts"]=settingsAttempts;d["uart_peripheral"]=useDiagnosticUart?1:2;d["uart_actual_baud"]=probePinsConfigured?radioUart().baudRate():0;String publicReply;for(unsigned i=0;i<settingsLength&&i<9;++i){char byte[3];snprintf(byte,sizeof(byte),"%02X",settingsBytes[i]);publicReply+=byte;}d["settings_response_hex"]=publicReply;auto pins=d["pin_levels"].to<JsonObject>();pins["initialized_by_probe"]=probePinsConfigured;pins["rx_gpio"]=config.lora_rx_pin;pins["tx_gpio"]=config.lora_tx_pin;pins["aux_gpio"]=config.lora_aux_pin;pins["m0_gpio"]=config.lora_m0_pin;pins["m1_gpio"]=config.lora_m1_pin;
// Direct pad reads, independent of driver Fault/Off readiness. No pin mode/output changes here.
pins["aux_high"]=digitalRead(config.lora_aux_pin)==HIGH;if(probePinsConfigured){pins["m0_high"]=digitalRead(config.lora_m0_pin)==HIGH;pins["m1_high"]=digitalRead(config.lora_m1_pin)==HIGH;}else{pins["m0_high"]=nullptr;pins["m1_high"]=nullptr;}
auto budget=d["airtime_budget"].to<JsonObject>();
budget["state"]=budgetBlocked?"BUDGET_EXHAUSTED":bootQuiet?"BOOT_QUIET":"AVAILABLE";
budget["used_us"]=airtime.used(millis());budget["limit_us"]=RadioAirtime::LimitUs;
budget["refusals"]=budgetDenials;budget["poll_interval_ms"]=RadioAirtime::PollIntervalMs;budget["model_verified"]=false;
d["tx_pending"]=txLength>0;d["tx_uart_fragments"]=txCount;d["rx_messages"]=rxCount;d["rejected"]=rejected;d["auth_failures"]=authFailures;d["timeouts"]=timeouts;d["reassembly_expired"]=assemblies.expired;d["reassembly_duplicates"]=assemblies.duplicates;d["completed_uart_transfers"]=completed;if(hasRssi&&observed.rssi&&rxCount)d["rssi_dbm"]=lastRssi;else d["rssi_dbm"]=nullptr;d["snr_db"]=nullptr;if(observedValid){auto s=d["observed"].to<JsonObject>();s["address"]=observed.address;s["baud"]=observed.baud;s["channel"]=observed.channel;s["air_rate_code"]=observed.airRate;s["power_dbm"]=observed.power;s["subpacket_bytes"]=observed.packet;s["wor_cycle_ms"]=observed.worMs;s["fixed_mode"]=observed.fixed;s["rssi_enabled"]=observed.rssi;s["lbt"]=observed.lbt;}String out;serializeJson(d,out);return out;}
