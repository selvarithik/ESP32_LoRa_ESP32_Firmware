#include "transport_manager.h"
#include "../core/config_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "canonical_mqtt.h"
#include "../config/mqtt_staging.h"
#include <atomic>
#include <time.h>
#include <memory>
#include <new>

TransportManager transportManager;
namespace {
struct Frame { char raw[MAX_FRAME_BYTES+1];char host[254];bool gateway; };
struct Result {char id[49];bool success;bool gateway;};
QueueHandle_t outgoing=nullptr,incoming=nullptr,results=nullptr;TaskHandle_t workerTask=nullptr;
DeviceConfig settings;String initialMqttProfile;uint32_t initialSharedRevision=0;
std::atomic<uint32_t> lastGateway{0},lastOk{0},overflows{0};
std::atomic<bool> mqttUp{false};
std::atomic<uint32_t> masterIp{0};
std::atomic<int> networkError{0};
std::atomic<int> probeCode{0};
std::atomic<uint32_t> probeAt{0};
class BoundedBody:public Stream {
public:
    String body; bool overflow=false;
    BoundedBody(){body.reserve(6144);}
    size_t write(uint8_t b) override {if(body.length()>=6144){overflow=true;return 0;}body+=(char)b;return 1;}
    int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}void flush() override{}
};
bool request(const Frame& f) {
    WiFiClient plain;WiFiClientSecure tls;HTTPClient http;
    bool secure=f.gateway&&settings.server_https;
    IPAddress resolved;
    if(!WiFi.hostByName(f.host,resolved)){networkError=-1001;return false;}
    if(!f.gateway&&String(f.host)==settings.master_host)masterIp=static_cast<uint32_t>(resolved);
    if(secure)tls.setCACert(settings.gateway_ca.c_str());
    String url=String(secure?"https://":"http://")+f.host+":"+String(f.gateway?settings.server_port:80)+(f.gateway?"/api/v1/device/exchange":"/api/v1/packets");
    http.setConnectTimeout(1500);http.setTimeout(1500);http.setReuse(false);
    if(!http.begin(secure?static_cast<WiFiClient&>(tls):plain,url))return false;
    http.addHeader("Content-Type","application/json");
    if(f.gateway) {http.addHeader("Authorization","Bearer "+settings.gateway_token);http.addHeader("X-Device-ID",settings.device_id);}
    else http.setAuthorization("mesh",settings.mesh_key.c_str());
    int code=http.POST((uint8_t*)f.raw,strlen(f.raw));
    networkError=code;
    bool ok=false;
    if(code==200||code==202) {
        BoundedBody sink;
        int size=http.getSize();
        if(size<=6144&&http.writeToStream(&sink)>=0&&!sink.overflow) {
            JsonDocument d;
            if(!deserializeJson(d,sink.body)&&d["accepted"].is<bool>()&&d["accepted"].as<bool>()) {
                ok=true;
                if(f.gateway) for(JsonVariant v:d["commands"].as<JsonArray>()) {String raw;serializeJson(v,raw);transportManager.enqueue(raw,"GATEWAY",true);}
            }
        }
    }
    http.end();return ok;
}
void worker(void*) {
    uint32_t lastProbe=millis()-60000;bool timeStarted=false,mqttStarted=false;
    for(;;) {
        bool wifi=WiFi.status()==WL_CONNECTED;
        if(wifi&&!timeStarted&&(!initialMqttProfile.isEmpty()||settings.server_https||settings.mqtt_tls||(!settings.internet_probe_host.isEmpty()&&settings.internet_probe_tls))){configTime(0,0,"pool.ntp.org");timeStarted=true;}
        if(wifi&&!settings.internet_probe_host.isEmpty()&&millis()-lastProbe>=60000) {
            lastProbe=millis();WiFiClient probePlain;WiFiClientSecure probeTls;HTTPClient probe;
            if(settings.internet_probe_tls)probeTls.setCACert(settings.internet_probe_ca.c_str());
            String url=String(settings.internet_probe_tls?"https://":"http://")+settings.internet_probe_host+":"+String(settings.internet_probe_port)+"/";
            probe.setConnectTimeout(1500);probe.setTimeout(1500);
            if(probe.begin(settings.internet_probe_tls?static_cast<WiFiClient&>(probeTls):probePlain,url)) {probeCode=probe.sendRequest("HEAD");probeAt=millis();probe.end();}
        }
        if(wifi&&!mqttStarted&&time(nullptr)>1700000000){CanonicalMqtt::begin(initialMqttProfile,initialSharedRevision);initialMqttProfile="";mqttStarted=true;}
        CanonicalMqtt::loop();mqttUp=CanonicalMqtt::connected();
        Frame* received=nullptr;
        if(xQueueReceive(outgoing,&received,pdMS_TO_TICKS(20))!=pdTRUE)continue;
        std::unique_ptr<Frame> owner(received);Frame& frame=*received;
        Message packet;bool ok=false;
        if(Message::fromJson(String(frame.raw),packet)&&wifi) {
            if(frame.gateway&&mqttUp) {CanonicalMqtt::publish(packet);} else if(!frame.gateway||(!settings.gateway_token.isEmpty()&&!settings.server_host.isEmpty()))ok=request(frame);
        }
        if(ok){lastOk=millis();if(frame.gateway)lastGateway=millis();}
        Result result{};strlcpy(result.id,packet.messageId.c_str(),sizeof(result.id));result.success=ok;result.gateway=frame.gateway;
        if(xQueueSend(results,&result,0)!=pdTRUE)++overflows;
    }
}
}
void TransportManager::begin() {
    settings=configManager.get();initialMqttProfile=mqttStaging.active();initialSharedRevision=mqttStaging.sharedRevision();
    outgoing=xQueueCreate(6,sizeof(Frame*));incoming=xQueueCreate(6,sizeof(Frame*));results=xQueueCreate(8,sizeof(Result));
    if(!outgoing||!incoming||!results){++overflows;return;}
    if(xTaskCreate(worker,"network",8192,nullptr,1,&workerTask)!=pdPASS)++overflows;
}
void TransportManager::loop() {}
bool TransportManager::sendLoRa(const Message&){return false;}
bool TransportManager::send(const Message& p,const String& host,bool gateway) {
    if(!outgoing||(!gateway&&host.isEmpty())||host.length()>253)return false;
    String raw=p.toJson();if(raw.length()>MAX_FRAME_BYTES||p.payload.length()>MAX_PAYLOAD_BYTES)return false;
    Frame* f=new(std::nothrow) Frame{};if(!f){++overflows;return false;}
    strlcpy(f->raw,raw.c_str(),sizeof(f->raw));strlcpy(f->host,host.c_str(),sizeof(f->host));f->gateway=gateway;
    if(xQueueSend(outgoing,&f,0)!=pdTRUE){delete f;++overflows;return false;}return true;
}
bool TransportManager::enqueue(const String& raw,const String& remote,bool gateway) {
    if(!incoming||raw.length()>MAX_FRAME_BYTES)return false;
    Frame* f=new(std::nothrow) Frame{};if(!f){++overflows;return false;}
    strlcpy(f->raw,raw.c_str(),sizeof(f->raw));strlcpy(f->host,remote.c_str(),sizeof(f->host));f->gateway=gateway;
    if(xQueueSend(incoming,&f,0)!=pdTRUE){delete f;++overflows;return false;}return true;
}
bool TransportManager::receive(Message& p,String& remote,bool& gateway) {
    if(!incoming)return false;Frame* received=nullptr;if(xQueueReceive(incoming,&received,0)!=pdTRUE)return false;
    std::unique_ptr<Frame> owner(received);remote=received->host;gateway=received->gateway;return Message::fromJson(String(received->raw),p);
}
bool TransportManager::delivery(String& id,bool& ok,bool& gateway) {if(!results)return false;Result r{};if(xQueueReceive(results,&r,0)!=pdTRUE)return false;id=r.id;ok=r.success;gateway=r.gateway;return true;}
uint32_t TransportManager::minimumStackFreeBytes() const{return workerTask?uxTaskGetStackHighWaterMark(workerTask):0;}
bool TransportManager::gatewayConnected() const {return lastGateway&&millis()-lastGateway.load()<90000;}
bool TransportManager::mqttConnected() const{return mqttUp;}
uint32_t TransportManager::lastSuccess() const{return lastOk;}
uint32_t TransportManager::dropped() const{return overflows;}
void TransportManager::gatewayReceipt(){lastGateway=millis();lastOk=millis();}
String TransportManager::masterAddress() const {return IPAddress(masterIp.load()).toString();}
int TransportManager::lastError() const{return networkError;}
String TransportManager::internetStatus() const {
    if(settings.internet_probe_host.isEmpty()||!probeAt||millis()-probeAt.load()>120000)return "UNKNOWN";
    if(WiFi.status()!=WL_CONNECTED)return "LOCAL_NETWORK_DISCONNECTED";
    int code=probeCode;return code<=0?"PROBE_UNREACHABLE":code>=200&&code<400?"REACHABLE":"REMOTE_SERVICE_UNAVAILABLE";
}
