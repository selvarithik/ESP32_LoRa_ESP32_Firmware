#pragma once
#include <WiFi.h>
// Common IP transport boundary. A board-specific Ethernet adapter may be added only
// after its PHY/SPI hardware and pins are known; no built-in Ethernet is assumed.
namespace IpNetwork {
inline bool available(){return WiFi.isConnected();}
inline const char* name(){return available()?"WIFI":"NONE";}
inline IPAddress address(){return WiFi.localIP();}
inline bool ethernetAvailable(){return false;}
}
