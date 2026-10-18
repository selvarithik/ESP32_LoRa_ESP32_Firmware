#pragma once
#include <Arduino.h>
class SensorManager {
public:
    void begin();
    void loop();
    String telemetryJson() const;
private:
    float temperature=0, humidity=0;
    bool valid=false;
    uint32_t sampled=0,lastGood=0;
};
extern SensorManager sensorManager;
