#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Main-loop-owned. This boundary deliberately cannot activate a broker.
class MqttStaging {
public:
    void begin();
    void loop();
    void control(const String& raw);
    int provision(const String& request,String& response,bool restoreInvalid=false);
    String active() const;
    uint32_t sharedRevision() const;
    String status(bool includeProfiles=true) const;
    int action(const String& operation,const String& request,String& response);
private:
    JsonDocument state;
    bool ready=false;
    bool save(const JsonDocument& candidate);
};
extern MqttStaging mqttStaging;
