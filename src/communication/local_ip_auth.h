#pragma once
#include <Arduino.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <esp_system.h>
namespace LocalIpAuth {
inline String mac(const String& secret,const String& domain,const String& id,const String& nonce,const String& body){
 unsigned char key[32],out[32];if(mbedtls_sha256_ret((const unsigned char*)secret.c_str(),secret.length(),key,0))return "";
 String raw=domain+"\n"+id+"\n"+nonce+"\n"+body;
 if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),key,32,(const unsigned char*)raw.c_str(),raw.length(),out))return "";
 char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",out[i]);return String(hex);
}
inline bool equal(const String& a,const String& b){if(a.length()!=64||b.length()!=64)return false;unsigned char diff=0;for(unsigned i=0;i<64;i++)diff|=a[i]^b[i];return diff==0;}
inline String nonce(){char value[33];for(unsigned i=0;i<4;i++)snprintf(value+8*i,9,"%08lx",(unsigned long)esp_random());return String(value);}
}
