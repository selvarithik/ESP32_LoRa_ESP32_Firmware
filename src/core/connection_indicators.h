#pragma once
#include <Arduino.h>
class ConnectionIndicators {
public:
 void begin();
 void loop();
 void stop();
 String statusJson() const;
};
extern ConnectionIndicators connectionIndicators;
