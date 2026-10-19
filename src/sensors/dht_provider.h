#pragma once
#include "../drivers/hardware_interfaces.h"
#include <DHT.h>
#include <cmath>
class DhtProvider:public Hardware::SensorProvider {
 DHT sensor;
public:explicit DhtProvider(int pin):sensor(pin,DHT11){}void begin(){sensor.begin();}
 Hardware::SensorSample read()override{Hardware::SensorSample r;r.temperature=sensor.readTemperature();r.humidity=sensor.readHumidity();r.valid=std::isfinite(r.temperature)&&std::isfinite(r.humidity)&&r.temperature>=0&&r.temperature<=50&&r.humidity>=0&&r.humidity<=100;return r;}
};
