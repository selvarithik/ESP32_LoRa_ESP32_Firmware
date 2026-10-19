#include "../src/protocol/protocol.h"
#include "../src/communication/local_ip_core.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
 if(argc==2){Message m;bool ok=Message::fromJson(String(argv[1]),m);std::cout<<(ok?m.toJson().c_str():"INVALID");return ok?0:2;}
 assert(LocalIpCore::gatewayCredential(true,false,true));
 assert(!LocalIpCore::gatewayCredential(false,false,true));
 assert(!LocalIpCore::direct(false,true,"MASTER_ONLY",true));
 assert(LocalIpCore::direct(false,true,"DIRECT_ONLY",true));
 assert(!LocalIpCore::direct(false,false,"DIRECT_ONLY",true));
 assert(LocalIpCore::fallback(true,true,"DIRECT_PREFERRED",true,true,true));
 assert(!LocalIpCore::fallback(true,true,"DIRECT_ONLY",true,true,true));
 assert(!LocalIpCore::fallback(true,true,"DIRECT_PREFERRED",false,true,true));
 Message m;m.messageId="MSG-HOST-1";m.source="IP-MASTER";m.origin=m.source;m.destination="GATEWAY";m.nextHop=m.destination;m.type=PacketType::TELEMETRY;m.timestamp=1800000000;m.sequence=3;m.payload="{\"valid\":true,\"parameters\":{\"temperature\":25.5,\"humidity\":55}}";
 std::cout<<m.toJson().c_str();return 0;
}
