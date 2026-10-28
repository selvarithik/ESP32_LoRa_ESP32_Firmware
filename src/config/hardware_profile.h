#pragma once
#if defined(DEVICE_ROLE_MASTER) && defined(DEVICE_ROLE_SLAVE)
#error Select exactly one device role
#endif
#if defined(INITIAL_MASTER_ROLE) && !defined(DEVICE_ROLE_MASTER)
#define DEVICE_ROLE_MASTER 1
#endif
namespace HardwareProfile {
#if defined(DEVICE_ROLE_MASTER)
constexpr bool ExplicitRole=true,Master=true;
#elif defined(DEVICE_ROLE_SLAVE)
constexpr bool ExplicitRole=true,Master=false;
#else
constexpr bool ExplicitRole=false,Master=false; // Legacy esp32dev target retains NVS-selected role.
#endif
constexpr int Dht=4,Mq135=34,InaSda=21,InaScl=22,RadioRx=16,RadioTx=17,RadioM0=25,RadioM1=26,RadioAux=27;
constexpr bool DisplayPresent=false;
inline bool roleMatches(bool storedMaster){return !ExplicitRole||storedMaster==Master;}
inline bool inaAllowed(bool storedMaster){return roleMatches(storedMaster)&&(!ExplicitRole||Master);}
}
