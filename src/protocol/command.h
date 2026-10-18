#pragma once
#include "protocol.h"

namespace CommandProtocol {
Message buildCommand(const String& source,
                     const String& destination,
                     const String& nextHop,
                     const String& command,
                     const String& commandId,
                     const String& payload,
                     uint8_t ttl);
}
