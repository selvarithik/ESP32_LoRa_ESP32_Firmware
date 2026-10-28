#pragma once
#include <stdint.h>
#include <string.h>
namespace DeliveryCore {
inline uint32_t retryAt(bool command,int code,uint32_t now,uint32_t scheduled){return !command&&code==503?now+1000:scheduled;}
// An authenticated parent may have rebooted and lost its runtime child table.
// Re-register on packet rejection, without weakening nonce/signature checks.
inline bool registrationRetry(bool node,bool gateway,int code,uint32_t elapsed){return node&&!gateway&&code==403&&elapsed>=5000;}
inline bool eligible(bool inFlight,bool command,bool acknowledged,uint32_t now,uint32_t next){return !inFlight&&!(command&&acknowledged)&&int32_t(now-next)>=0;}
inline bool replaceTelemetry(bool gateway,bool localSource,bool inFlight,const char* sensor,const char* pendingSensor){return gateway&&localSource&&!inFlight&&sensor&&sensor[0]&&pendingSensor&&strcmp(sensor,pendingSensor)==0;}
// A verified HTTP QUEUED receipt hands non-command ownership to the peer. It
// does not claim Gateway persistence or completed device-command execution.
inline bool transportReceiptCompletes(bool command,bool gateway,bool radio){return !command&&(gateway||!radio);}
inline bool applicationReceiptRequired(bool localIp,bool radio){return radio||!localIp;}
}
