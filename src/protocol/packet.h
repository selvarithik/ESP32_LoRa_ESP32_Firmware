#pragma once
#include "protocol.h"

namespace Packet {
Message make(PacketType type,
             const String& source,
             const String& destination,
             const String& nextHop,
             const String& payload,
             uint8_t ttl);
}
