#pragma once
#include <string.h>
namespace LocalIpCore {
inline bool gatewayCredential(bool localEnabled,bool legacy,bool dedicated){return legacy||(localEnabled&&dedicated);}
inline bool direct(bool master,bool enabled,const char* policy,bool configured){return master||(enabled&&strcmp(policy,"MASTER_ONLY")&&configured);}
inline bool fallback(bool node,bool enabled,const char* policy,bool failed,bool viaGateway,bool masterConfigured){return node&&enabled&&!strcmp(policy,"DIRECT_PREFERRED")&&failed&&viaGateway&&masterConfigured;}
}
