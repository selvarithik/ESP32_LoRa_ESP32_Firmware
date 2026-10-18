#include "../src/config/mqtt_digest_core.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc<7)return 2;std::vector<std::string> certificates;
 for(int i=6;i<argc;i++)certificates.push_back(argv[i]);
 std::cout<<MqttDigestCore::canonical(argv[1],std::stoul(argv[2]),argv[3],argv[4],std::stoul(argv[5]),certificates);
}
