#pragma once
#include <algorithm>
#include <cctype>
#include <stdint.h>
#include <string>
#include <vector>
// Portable serialization shared by the actual ESP32 digest and host acceptance tests.
// Inputs have already passed canonical profile validation (ASCII compared fields).
namespace MqttDigestCore {
inline std::string canonical(std::string host,uint32_t port,const std::string& prefix,
 const std::string& gateway,uint32_t revision,std::vector<std::string> fingerprints){
 for(auto& c:host)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
 std::sort(fingerprints.begin(),fingerprints.end());fingerprints.erase(std::unique(fingerprints.begin(),fingerprints.end()),fingerprints.end());
 std::string s="{\"schema_version\":1,\"shared_revision\":"+std::to_string(revision)+",\"values\":{\"ca_fingerprints\":[";
 bool first=true;for(auto& f:fingerprints){if(!first)s+=",";first=false;s+="\""+f+"\"";}
 return s+"],\"gateway_id\":\""+gateway+"\",\"host\":\""+host+"\",\"mqtt_version\":\"3.1.1\",\"port\":"+std::to_string(port)+",\"qos_default\":1,\"tls\":true,\"topic_prefix\":\""+prefix+"\"}}";
}
}
