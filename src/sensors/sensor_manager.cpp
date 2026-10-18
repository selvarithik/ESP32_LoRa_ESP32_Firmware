#include "sensor_manager.h"
#include "../core/config_manager.h"
#include <ArduinoJson.h>
#include <DHT.h>
#include <cmath>

SensorManager sensorManager;
namespace {
struct Reading {float t,h;bool valid;uint32_t sampled,lastGood;};
QueueHandle_t readings=nullptr;
int pin=4;
void sample(void*) {
    DHT dht(pin,DHT11);dht.begin();uint32_t lastGood=0;
    for(;;) {vTaskDelay(pdMS_TO_TICKS(2000));Reading r{};r.t=dht.readTemperature();r.h=dht.readHumidity();r.sampled=millis();r.valid=std::isfinite(r.t)&&std::isfinite(r.h)&&r.t>=0&&r.t<=50&&r.h>=0&&r.h<=100;if(r.valid)lastGood=r.sampled;r.lastGood=lastGood;xQueueOverwrite(readings,&r);}
}
}
void SensorManager::begin() {
    if(!configManager.get().dht_enabled)return;
    pin=configManager.get().dht_pin;readings=xQueueCreate(1,sizeof(Reading));
    if(readings&&xTaskCreate(sample,"dht11",3072,nullptr,1,nullptr)!=pdPASS){vQueueDelete(readings);readings=nullptr;}
}
void SensorManager::loop() {if(!readings)return;Reading r;if(xQueueReceive(readings,&r,0)==pdTRUE){temperature=r.t;humidity=r.h;valid=r.valid;sampled=r.sampled;lastGood=r.lastGood;}}
String SensorManager::telemetryJson() const {
    JsonDocument d;d["sensor_id"]=configManager.get().device_id+"-DHT11";d["sensor_type"]="DHT11";
    bool good=valid&&millis()-sampled<10000;
    d["valid"]=good;d["status"]=!configManager.get().dht_enabled?"NOT_CONFIGURED":!readings?"TASK_UNAVAILABLE":!sampled?"WAITING":good?"OK":"READ_ERROR";
    d["sampled_uptime_ms"]=sampled;d["last_success_uptime_ms"]=lastGood;d["sampling_interval_seconds"]=2;
    auto params=d["parameters"].to<JsonObject>();if(good){params["temperature"]=temperature;params["humidity"]=humidity;}
    d["units"]["temperature"]="C";d["units"]["humidity"]="%RH";
    String s;serializeJson(d,s);return s;
}
