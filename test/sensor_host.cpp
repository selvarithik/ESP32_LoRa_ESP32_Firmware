#include <Arduino.h>
#include "../src/sensors/telemetry_validation.h"
#include <iostream>
int main(int argc,char** argv){JsonDocument d;if(argc!=2||deserializeJson(d,argv[1])||!d.is<JsonObject>())return 2;bool ok=SensorTelemetry::valid(d.as<JsonObjectConst>());std::cout<<(ok?"VALID":"INVALID");return ok?0:2;}
