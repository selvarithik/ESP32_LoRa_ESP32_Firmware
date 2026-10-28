#include "local_web_server.h"
#include "../communication/local_ip_auth.h"
#include "../communication/transport_manager.h"
#include "../core/network_service.h"
#include "../core/config_manager.h"
#include "../config/strict_json.h"
#include "../communication/http_body_core.h"
#include <time.h>
namespace {
struct Challenge {String id,nonce;uint32_t issued=0;};Challenge challenges[8];unsigned challengeIndex=0;
bool permitted(const String& id){
 auto& c=configManager.get();if(!c.local_ip_enabled||c.mesh_key.length()<32||id.isEmpty()||id.length()>48)return false;
 for(char v:id)if(!isalnum(v)&&v!='_'&&v!='-'&&v!='.'&&v!=':')return false;
 if(c.role==DeviceRole::NODE)return id==c.master_id;
 String allowed=","+c.allowed_peers_csv+",";allowed.replace(" ","");return allowed.indexOf(","+id+",")>=0;
}
}
void LocalWebServer::readLocalBody(){
 auto& chunk=_server.raw();
 if(chunk.status==RAW_START){localBody="";localBodyRejected=false;
   if(_server.clientContentLength()<=0||_server.clientContentLength()>MAX_FRAME_BYTES||_server.header("Transfer-Encoding").length()||_server.header("Content-Type")!="application/json"){
     localBodyRejected=true;sendJson(413,"{\"error\":\"INVALID_FRAME_LENGTH\"}");_server.bodyClient().stop();return;
   }
   localBody.reserve(_server.clientContentLength());
   struct Sink {String& body;size_t write(const uint8_t* bytes,size_t count){return body.concat((const char*)bytes,count)?count:0;}} sink{localBody};
   // The pinned SDK's raw parser asks for HTTP_RAW_BUFLEN even for the final
   // short chunk. Consume exactly Content-Length here with an absolute deadline.
   bool ok=HttpBodyCore::read(_server.bodyClient(),sink,_server.clientContentLength(),MAX_FRAME_BYTES,1500,[](){return millis();},[](){delay(1);});
   chunk.totalSize=_server.clientContentLength(); // Skip SDK's unbounded tail read.
   if(!ok){localBody="";localBodyRejected=true;sendJson(408,"{\"error\":\"BODY_INCOMPLETE_OR_TIMEOUT\"}");_server.bodyClient().stop();}
 }
}
void LocalWebServer::handleLocalChallenge(){
 String id=_server.header("X-Device-ID");if(!permitted(id)){sendJson(401,"{\"error\":\"AUTH_FAILED\"}");return;}
 auto& c=challenges[challengeIndex++%8];c.id=id;c.nonce=LocalIpAuth::nonce();c.issued=millis();JsonDocument d;d["nonce"]=c.nonce;uint32_t stamp=time(nullptr)>1700000000?time(nullptr):0;d["unix_time"]=stamp;
 d["signature"]=LocalIpAuth::mac(configManager.get().mesh_key,"LOCALIP1-TIME",id,c.nonce,String(stamp));String raw;serializeJson(d,raw);sendJson(200,raw);
}
void LocalWebServer::handleLocalExchange(){
 if(localBodyRejected)return;
 String id=_server.header("X-Device-ID"),nonce=_server.header("X-Local-Nonce");bool found=false;
 for(auto& c:challenges)if(c.nonce==nonce&&c.id==id&&nonce.length()==32){found=millis()-c.issued<=30000;c=Challenge{};break;}
 if(!found||!permitted(id)){sendJson(401,"{\"error\":\"AUTH_FAILED\"}");return;}
 String raw=localBody;localBody="";if(raw.isEmpty()||raw.length()!=size_t(_server.clientContentLength())){sendJson(400,"{\"error\":\"INVALID_FRAME_LENGTH\"}");return;}
 if(!LocalIpAuth::equal(LocalIpAuth::mac(configManager.get().mesh_key,"LOCALIP1",id,nonce,raw),_server.header("X-Local-Signature"))){sendJson(401,"{\"error\":\"AUTH_FAILED\"}");return;}
 Message m;String remote=_server.client().remoteIP().toString();
 if(!Message::fromJson(raw,m)||!networkService.accept(m,remote)||((m.source!=id)&&(configManager.get().role!=DeviceRole::NODE||m.source!="GATEWAY"))){sendJson(403,"{\"error\":\"INVALID_OR_UNAUTHORIZED_PACKET\"}");return;}
 auto now=time(nullptr);if(m.timestamp&&now>1700000000&&(m.timestamp>now+30||now-m.timestamp>300)){sendJson(400,"{\"error\":\"EXPIRED\"}");return;}
 if((m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)&&(!m.timestamp||now<1700000000)){sendJson(409,"{\"error\":\"CLOCK_REQUIRED\"}");return;}
 if(!transportManager.enqueue(raw,remote)){_server.sendHeader("Retry-After","1");sendJson(503,String("{\"error\":\"")+transportManager.inboundError()+"\"}");return;}
 JsonDocument d;d["schema_version"]=1;d["accepted"]=true;d["received_message_id"]=m.messageId;d["receipt"]="QUEUED";String reply;serializeJson(d,reply);
 _server.sendHeader("X-Local-Signature",LocalIpAuth::mac(configManager.get().mesh_key,"LOCALIP1-REPLY",id,nonce,reply));sendJson(202,reply);
}
