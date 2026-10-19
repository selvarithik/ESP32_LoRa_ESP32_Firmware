#pragma once
#include <Arduino.h>
class SensorManager {
public:
    void begin();
    void loop();
    String telemetryJson(unsigned index=0) const;
    String healthJson() const;
};
extern SensorManager sensorManager;
