#pragma once
#include <string.h>
#include <stdint.h>
namespace LocalIpCore {
inline bool fresh(uint32_t now,uint32_t receipt,uint32_t timeout=15000){return receipt&&uint32_t(now-receipt)<timeout;}
inline const char* state(bool enabled,bool link,int phase,uint32_t now,uint32_t receipt){
 if(!enabled)return "DISABLED";if(!link)return "LINK_DOWN";if(phase==3)return "AUTH_FAILED";
 if(fresh(now,receipt))return "CONNECTED";
 return phase==1?"CONNECTING":phase==4?"CONNECTION_OR_TIMEOUT_FAILED":phase==5?"PROTOCOL_FAILED":"DISCONNECTED";
}
inline bool credential(bool master,const char* policy,bool gateway,bool mesh){return !master&&!strcmp(policy,"MASTER_ONLY")?mesh:gateway;}
inline bool gatewayCredential(bool localEnabled,bool legacy,bool dedicated){return legacy||(localEnabled&&dedicated);}
inline bool direct(bool master,bool enabled,const char* policy,bool configured){return master||(enabled&&strcmp(policy,"MASTER_ONLY")&&configured);}
inline bool fallback(bool node,bool enabled,const char* policy,bool failed,bool viaGateway,bool masterConfigured){return node&&enabled&&!strcmp(policy,"DIRECT_PREFERRED")&&failed&&viaGateway&&masterConfigured;}
}
