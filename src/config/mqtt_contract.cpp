#include "mqtt_contract.h"
#include <mbedtls/x509_crt.h>
#include <cstring>


namespace MqttContract {
namespace {
bool asciiId(const String& s, size_t maximum, bool prefix = false) {
    if(s.isEmpty() || s.length()>maximum || (prefix && (s[0]=='/' || s.endsWith("/"))))return false;
    char previous=0;
    for(size_t i=0;i<s.length();++i){char c=s[i];bool alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
        if(!alnum&&c!='_'&&c!='-'&&!(prefix?c=='/':c=='.'||c==':'))return false;
        if(prefix&&c=='/'&&previous=='/')return false;previous=c;}
    return true;
}
bool integer(JsonVariantConst v, uint32_t minimum, uint32_t maximum) {
    return !v.is<bool>()&&v.is<uint32_t>()&&v.as<uint32_t>()>=minimum&&v.as<uint32_t>()<=maximum;
}
bool text(JsonVariantConst v,size_t minimum,size_t maximum){return v.is<const char*>()&&v.as<JsonString>().size()==strlen(v.as<const char*>())&&v.as<JsonString>().size()>=minimum&&v.as<JsonString>().size()<=maximum;}
bool hostname(const String& host){
    String lower=host;lower.toLowerCase();
    if(host.isEmpty()||host.length()>253||lower=="localhost"||host=="0.0.0.0"||host.startsWith("127.")||host.endsWith("."))return false;
    size_t label=0;char previous=0;
    for(size_t i=0;i<host.length();++i){char c=host[i];
        if(c=='.'){if(!label||previous=='-')return false;label=0;}
        else {if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')||(!label&&c=='-')||++label>63)return false;}
        previous=c;}
    return label&&previous!='-';
}
}
void defaults(JsonObject p){
    p["profile_name"]="Primary Broker";p["provider"]="EMQX";p["host"]="";p["port"]=8883;
    p["mqtt_version"]="3.1.1";p["tls"]=true;p["ca_cert"]="";p["username"]="";p["password"]="";
    p["client_id"]="";p["keepalive_s"]=60;p["timeout_s"]=10;p["qos_default"]=1;
    p["topic_prefix"]="selvarithik/gateway";p["gateway_id"]="GW001";
}
bool isPreservedSecret(JsonVariantConst value){
    if(value.isNull())return true;if(!value.is<const char*>())return false;
    String s=value.as<String>();if(s.isEmpty())return true;
    for(size_t i=0;i<s.length();++i)if(s[i]!='*')return false;
    return true;
}
bool validate(JsonObjectConst p,String& field,bool connectionReady){
    for(JsonPairConst pair:p){bool known=false;for(auto name:fields)if(strcmp(name,pair.key().c_str())==0)known=true;
        if(!known){field="profile";return false;}}
    for(auto name:fields)if(!p[name].is<JsonVariantConst>()||p[name].isNull()){field=name;return false;}
#define CHECK(key, condition) if(!(condition)){field=key;return false;}
    CHECK("profile_name",text(p["profile_name"],1,64));
    String provider=p["provider"]|"";
    CHECK("provider",provider=="EMQX"||provider=="HIVEMQ"||provider=="MOSQUITTO"||provider=="CUSTOM");
    CHECK("host",text(p["host"],1,253)&&hostname(p["host"].as<String>()));
    CHECK("port",integer(p["port"],1,65535));
    CHECK("mqtt_version",p["mqtt_version"].is<const char*>()&&p["mqtt_version"].as<String>()=="3.1.1");
    CHECK("tls",p["tls"].is<bool>()&&p["tls"].as<bool>());
    CHECK("ca_cert",text(p["ca_cert"],1,4096));
    String pem=p["ca_cert"].as<String>();
    CHECK("ca_cert",pem.indexOf("PRIVATE KEY")==-1&&pem.startsWith("-----BEGIN CERTIFICATE-----"));
    mbedtls_x509_crt chain;mbedtls_x509_crt_init(&chain);
    int parsed=mbedtls_x509_crt_parse(&chain,(const unsigned char*)pem.c_str(),pem.length()+1);
    bool ca=parsed==0;for(auto cert=&chain;cert&&cert->raw.p;cert=cert->next)ca=ca&&cert->ca_istrue;
    mbedtls_x509_crt_free(&chain);CHECK("ca_cert",ca);
    CHECK("username",text(p["username"],connectionReady?1:0,128));
    CHECK("password",text(p["password"],connectionReady?1:0,256));
    CHECK("client_id",text(p["client_id"],1,64)&&asciiId(p["client_id"].as<String>(),64));
    CHECK("keepalive_s",integer(p["keepalive_s"],15,300));
    CHECK("timeout_s",integer(p["timeout_s"],2,30));
    CHECK("qos_default",integer(p["qos_default"],1,1));
    CHECK("topic_prefix",text(p["topic_prefix"],1,128)&&asciiId(p["topic_prefix"].as<String>(),128,true));
    CHECK("gateway_id",text(p["gateway_id"],1,48)&&asciiId(p["gateway_id"].as<String>(),48));
#undef CHECK
    field="";return true;
}
bool topic(JsonObjectConst p,const String& type,String& result){
    result="";bool known=false;for(auto name:messageTypes)if(type==name)known=true;
    String prefix=p["topic_prefix"]|"",id=p["gateway_id"]|"";
    if(!known||!asciiId(prefix,128,true)||!asciiId(id,48))return false;
    result=prefix+"/"+id+"/"+type;return true;
}
}
