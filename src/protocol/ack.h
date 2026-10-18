#pragma once
#include "protocol.h"

namespace AckProtocol {
Message buildAck(const String& source,
                 const String& destination,
                 const String& nextHop,
                 const String& commandId,
                 const String& status,
                 const String& message,
                 uint8_t ttl);
}
