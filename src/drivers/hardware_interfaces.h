#pragma once
#include <stdint.h>
#include <stddef.h>
namespace Hardware {
struct SensorSample {float temperature=0,humidity=0;bool valid=false,simulated=false;};
class SensorProvider {public:virtual ~SensorProvider(){};virtual SensorSample read()=0;};
struct RadioMetrics {uint32_t transmitted=0,received=0,rejected=0;bool rssiAvailable=false,snrAvailable=false;};
class RadioDriver {public:virtual ~RadioDriver(){};virtual bool begin()=0;virtual bool send(const uint8_t*,size_t)=0;virtual size_t receive(uint8_t*,size_t)=0;virtual void stop()=0;virtual RadioMetrics metrics() const=0;};
struct NetworkState {bool connected=false,internetKnown=false,internet=false,gateway=false,simulated=false;};
class NetworkDriver {public:virtual ~NetworkDriver(){};virtual NetworkState state() const=0;virtual bool reconnect()=0;virtual void disconnect()=0;};
}
