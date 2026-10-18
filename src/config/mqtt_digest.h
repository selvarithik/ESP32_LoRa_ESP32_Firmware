#pragma once
#include "mqtt_contract.h"
#include "mqtt_digest_core.h"
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <algorithm>
#include <vector>
namespace MqttDigest {
inline String hash(const unsigned char* bytes,size_t size){unsigned char h[32];mbedtls_sha256_ret(bytes,size,h,0);String r;for(auto b:h){char s[3];snprintf(s,3,"%02x",b);r+=s;}return r;}
inline String hash(const String& raw){return hash((const unsigned char*)raw.c_str(),raw.length());}
inline bool snapshot(JsonObjectConst p,uint32_t revision,JsonDocument& out){
 String field;if(!MqttContract::validate(p,field))return false;
 String pem=p["ca_cert"].as<String>();mbedtls_x509_crt chain;mbedtls_x509_crt_init(&chain);
 if(mbedtls_x509_crt_parse(&chain,(const unsigned char*)pem.c_str(),pem.length()+1)){mbedtls_x509_crt_free(&chain);return false;}
 std::vector<String> fingerprints;for(auto c=&chain;c&&c->raw.p;c=c->next)fingerprints.push_back(hash(c->raw.p,c->raw.len));mbedtls_x509_crt_free(&chain);
 std::sort(fingerprints.begin(),fingerprints.end());fingerprints.erase(std::unique(fingerprints.begin(),fingerprints.end()),fingerprints.end());
 std::vector<std::string> certs;for(auto& f:fingerprints)certs.emplace_back(f.c_str());
 auto raw=MqttDigestCore::canonical(p["host"].as<const char*>(),p["port"].as<uint32_t>(),p["topic_prefix"].as<const char*>(),p["gateway_id"].as<const char*>(),revision,certs);
 out.clear();return !deserializeJson(out,raw.c_str());
}
inline String digest(const JsonDocument& snapshot){String raw;serializeJson(snapshot,raw);return hash(raw);}
}
