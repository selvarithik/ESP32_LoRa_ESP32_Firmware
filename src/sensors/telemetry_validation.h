#pragma once
#include <ArduinoJson.h>
#include <cmath>
namespace SensorTelemetry {
inline bool valid(JsonObjectConst body){
 if(!body["valid"].is<bool>()||!body["parameters"].is<JsonObjectConst>())return false;
 if(!body["sensor_type"].isUnbound()&&!body["sensor_type"].is<const char*>())return false;
 auto p=body["parameters"].as<JsonObjectConst>();String type=body["sensor_type"]|"DHT11";
 if(type!="DHT11"&&type!="INA219"&&type!="MQ135")return false;
 if(!body["valid"].as<bool>())return p.size()==0;
 if(type=="DHT11"&&(p.size()!=2||p["temperature"].isUnbound()||p["humidity"].isUnbound()))return false;
 if(type=="MQ135"&&(p.size()!=2||p["mq135_adc_raw"].isUnbound()||p["mq135_pin_mv"].isUnbound()))return false;
 if(type=="INA219"&&((p.size()!=2&&p.size()!=4)||p["bus_voltage"].isUnbound()||p["shunt_voltage"].isUnbound()||(p.size()==4&&(p["current"].isUnbound()||p["power"].isUnbound()))))return false;
 for(JsonPairConst pair:p){String k=pair.key().c_str();auto v=pair.value();if(v.is<bool>()||!v.is<float>())return false;float n=v.as<float>();if(!std::isfinite(n))return false;
 if(type=="DHT11"){if(n<0||n>(k=="temperature"?50:100))return false;}
 else if(type=="INA219"){if(k=="bus_voltage"){if(n<0||n>26)return false;}else if(k=="shunt_voltage"){if(fabs(n)>=320)return false;}else if(k=="current"){if(fabs(n)>320)return false;}else if(k=="power"){if(fabs(n)>8320)return false;}else return false;}
 else {if(!v.is<uint32_t>()||n>(k=="mq135_adc_raw"?4095:3300))return false;}
 }return true;
}
}
