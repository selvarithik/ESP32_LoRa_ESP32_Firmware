#include "sensor_manager.h"
#include "../core/config_manager.h"
#include <ArduinoJson.h>
#include "dht_provider.h"
#include <Wire.h>
#include <cmath>

SensorManager sensorManager;
namespace {
struct Reading {float t=0,h=0,bus=0,shunt=0;uint32_t sampled=0,lastGood=0,adc=0,mv=0;bool dht=false,ina=false,mq=false;};
Reading latest;
QueueHandle_t readings=nullptr;
bool dhtEnabled=false,inaEnabled=false,mqEnabled=false;int dhtPin=4,address=64;uint32_t warmup=86400;
bool reg(uint8_t r,uint16_t& v){Wire.beginTransmission(address);Wire.write(r);if(Wire.endTransmission(false))return false;if(Wire.requestFrom(address,2)!=2)return false;v=(Wire.read()<<8)|Wire.read();return true;}
void sample(void*) {
    DhtProvider provider(dhtPin);if(dhtEnabled)provider.begin();
    if(inaEnabled){Wire.begin(21,22,100000);Wire.setTimeOut(50);Wire.beginTransmission(address);Wire.write(0);Wire.write(0x39);Wire.write(0x9f);Wire.endTransmission();}
    if(mqEnabled){pinMode(34,INPUT);analogReadResolution(12);analogSetPinAttenuation(34,ADC_11db);}
    uint32_t lastGood=0;
    for(;;){vTaskDelay(pdMS_TO_TICKS(2000));Reading r;r.sampled=millis();
        if(dhtEnabled){auto v=provider.read();r.t=v.temperature;r.h=v.humidity;r.dht=v.valid;if(r.dht)lastGood=r.sampled;}r.lastGood=lastGood;
        if(inaEnabled){uint16_t b,s;if(reg(2,b)&&reg(1,s)&&(b&2)&&!(b&1)){r.bus=(b>>3)*0.004f;r.shunt=(int16_t)s*0.01f;r.ina=r.bus<=26.0f&&fabs(r.shunt)<320.0f;}}
        if(mqEnabled){r.adc=analogRead(34);r.mv=analogReadMilliVolts(34);r.mq=true;}
        xQueueOverwrite(readings,&r);
    }
}
}
void SensorManager::begin(){auto& c=configManager.get();dhtEnabled=c.dht_enabled;inaEnabled=c.ina219_enabled;mqEnabled=c.mq135_enabled;dhtPin=c.dht_pin;address=c.ina219_address;warmup=c.mq135_warmup_seconds;
    if(!dhtEnabled&&!inaEnabled&&!mqEnabled)return;readings=xQueueCreate(1,sizeof(Reading));
    if(readings&&xTaskCreate(sample,"sensors",4096,nullptr,1,nullptr)!=pdPASS){vQueueDelete(readings);readings=nullptr;}
}
void SensorManager::loop(){if(readings){Reading r;if(xQueueReceive(readings,&r,0)==pdTRUE)latest=r;}}
String SensorManager::telemetryJson(unsigned index)const{
    JsonDocument d;const char* type=index==1?"INA219":index==2?"MQ135":"DHT11";d["sensor_type"]=type;d["sensor_id"]=configManager.get().device_id+"-"+type;
    bool enabled=index==1?inaEnabled:index==2?mqEnabled:dhtEnabled;bool fresh=latest.sampled&&millis()-latest.sampled<10000;
    bool good=enabled&&fresh&&(index==1?latest.ina:index==2?latest.mq:latest.dht);
    d["valid"]=good;d["simulated"]=false;d["status"]=!enabled?"NOT_CONFIGURED":!readings?"TASK_UNAVAILABLE":!latest.sampled?"WAITING":!good?"READ_ERROR":index==2?"UNCALIBRATED_RAW_ONLY":"OK";
    d["sampled_uptime_ms"]=enabled?latest.sampled:0;d["sampling_interval_seconds"]=2;
    auto p=d["parameters"].to<JsonObject>();auto u=d["units"].to<JsonObject>();
    if(index==0){d["last_success_uptime_ms"]=latest.lastGood;u["temperature"]="C";u["humidity"]="%RH";if(good){p["temperature"]=latest.t;p["humidity"]=latest.h;}}
    else if(index==1){u["bus_voltage"]="V";u["shunt_voltage"]="mV";uint16_t resistance=configManager.get().ina219_shunt_milliohms;d["current_calibrated"]=false;d["shunt_resistance_configured"]=resistance>0;d["shunt_milliohms"]=resistance;if(good){p["bus_voltage"]=latest.bus;p["shunt_voltage"]=latest.shunt;if(resistance){p["current"]=latest.shunt/resistance;p["power"]=latest.bus*(latest.shunt/resistance);u["current"]="A";u["power"]="W";}else d["status"]="SHUNT_RESISTANCE_REQUIRED";}}
    else {u["mq135_adc_raw"]="count";u["mq135_pin_mv"]="mV";d["calibrated"]=false;d["warmup_remaining_seconds"]=millis()/1000<warmup?warmup-millis()/1000:0;d["voltage_divider_verification_required"]=true;if(good){p["mq135_adc_raw"]=latest.adc;p["mq135_pin_mv"]=latest.mv;if(latest.adc>=4090)d["status"]="ADC_SATURATED";else if(millis()/1000<warmup)d["status"]="WARMING_UP_RAW_ONLY";}}
    String raw;serializeJson(d,raw);return raw;
}
String SensorManager::healthJson()const{JsonDocument d;auto a=d.to<JsonArray>();for(unsigned i=0;i<3;++i){JsonDocument s;deserializeJson(s,telemetryJson(i));s.remove("parameters");s.remove("units");a.add(s);}String raw;serializeJson(d,raw);return raw;}
