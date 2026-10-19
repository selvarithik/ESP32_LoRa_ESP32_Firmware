#include "mqtt_catalog.h"
#include "mqtt_contract.h"
#include "mqtt_staging.h"
#include "strict_json.h"
#include <Preferences.h>
#include <ArduinoJson.h>
namespace MqttCatalog {
namespace {int failure(int status,const char* code,String& response){response=String("{\"error\":{\"code\":\"")+code+"\"}}";return status;}}
int handle(const String& operation,const String& request,String& response){
 JsonDocument state;state["schema_version"]=1;state["catalog_revision"]=0;state["profiles"].to<JsonArray>();Preferences storage;
 if(!storage.begin("mqtt_catalog",false))return failure(503,"STORAGE_FAILED",response);
 if(storage.isKey("profiles")){String raw=storage.getString("profiles");if(raw.length()>22000||deserializeJson(state,raw)||!state["catalog_revision"].is<uint32_t>()||!state["profiles"].is<JsonArray>()){storage.end();return failure(503,"CATALOG_RECOVERY_REQUIRED",response);}}
 if(operation=="read"){serializeJson(state,response);storage.end();return 200;}
 JsonDocument input;if(!StrictJson::valid(request,8192)||deserializeJson(input,request)||!input["catalog_revision"].is<uint32_t>()||input["catalog_revision"].is<bool>()||input["catalog_revision"].as<uint32_t>()!=state["catalog_revision"].as<uint32_t>()){storage.end();return failure(409,"REVISION_CONFLICT",response);}
 auto profiles=state["profiles"].as<JsonArray>();String name=input["profile_name"]|"";
 if(operation=="save"){
  auto profile=input["profile"].as<JsonObject>();if(input.size()!=2||profile.isNull()||!MqttContract::isPreservedSecret(profile["password"])){storage.end();return failure(400,"PROTECTED_PROVISIONING_REQUIRED",response);}
  profile["password"]="";String field;if(!MqttContract::validate(profile,field)){storage.end();return failure(400,"INVALID_PROFILE",response);}profile.remove("password");name=profile["profile_name"].as<String>();bool found=false;for(JsonObject old:profiles)if(old["profile_name"].as<String>()==name){old.set(profile);found=true;break;}if(!found){if(profiles.size()>=4){storage.end();return failure(409,"CATALOG_FULL",response);}profiles.add(profile);}
 }else if(operation=="select"||operation=="delete"){
  if(input.size()!=(operation=="select"?3:3)){storage.end();return failure(400,"INVALID_REQUEST",response);}int index=-1;for(unsigned i=0;i<profiles.size();++i)if(profiles[i]["profile_name"].as<String>()==name){index=i;break;}if(index<0){storage.end();return failure(404,"PROFILE_NOT_FOUND",response);}
  if(operation=="select"){JsonDocument candidate;candidate["expected_revision"]=input["expected_revision"];candidate["profile"]=profiles[index];candidate["profile"]["password"]="";String raw;serializeJson(candidate,raw);storage.end();return mqttStaging.action("candidate",raw,response);}
  JsonDocument active;deserializeJson(active,mqttStaging.active());if(input["confirm_profile_name"].as<String>()!=name){storage.end();return failure(400,"CONFIRMATION_REQUIRED",response);}if(active["profile_name"].as<String>()==name){storage.end();return failure(409,"ACTIVE_PROFILE_PROTECTED",response);}profiles.remove(index);
 }else{storage.end();return failure(404,"UNKNOWN_OPERATION",response);}
 uint32_t revision=state["catalog_revision"];if(revision==0xffffffff){storage.end();return failure(409,"REVISION_CONFLICT",response);}state["catalog_revision"]=revision+1;String raw;serializeJson(state,raw);if(storage.putString("profiles",raw)!=raw.length()){storage.end();return failure(503,"STORAGE_FAILED",response);}storage.end();response=raw;return 200;
}
}
