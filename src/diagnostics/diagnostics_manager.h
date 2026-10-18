#pragma once
#include <Arduino.h>

class DiagnosticsManager {
public:
    void begin();
    String statusJson() const;
};

extern DiagnosticsManager diagnosticsManager;
