#pragma once
#include "protocol.h"

namespace ResponseProtocol {
Message buildResponse(const String& source,
                      const String& destination,
                      const String& nextHop,
                      const String& commandId,
                      const String& status,
                      const String& message,
                      const String& data,
                      uint8_t ttl);
}
