#include "packet.h"
#include <time.h>

namespace Packet {
Message make(PacketType type,
             const String& source,
             const String& destination,
             const String& nextHop,
             const String& payload,
             uint8_t ttl) {
    Message m;
    m.protocolVersion = 1;
    m.messageId = generateMessageId();
    m.source = source;
    m.destination = destination;
    m.nextHop = nextHop;
    m.type = type;
    m.payload = payload;
    m.ttl = ttl;
    time_t utc=time(nullptr);
    m.timestamp = utc>1600000000?static_cast<uint32_t>(utc):0;
    return m;
}
}
