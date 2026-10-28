#include "../src/communication/delivery_core.h"
#include "../src/communication/local_ip_core.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(DeliveryCore::retryAt(false,503,1000,6000)==2000);
 assert(DeliveryCore::retryAt(true,503,1000,6000)==6000);
 assert(DeliveryCore::retryAt(false,401,1000,6000)==6000);
 assert(DeliveryCore::retryAt(false,503,0xffffff00U,0)==744);

 assert(DeliveryCore::registrationRetry(true,false,403,5000));
 assert(!DeliveryCore::registrationRetry(true,false,403,4999));
 assert(!DeliveryCore::registrationRetry(true,false,401,10000));
 assert(!DeliveryCore::registrationRetry(false,false,403,10000));
 assert(!DeliveryCore::registrationRetry(true,true,403,10000));

 assert(!strcmp(LocalIpCore::state(true,true,1,1500,1000),"CONNECTED")); // In-flight work is not a disconnect.
 assert(!strcmp(LocalIpCore::state(true,true,1,16000,1000),"CONNECTING"));
 assert(!strcmp(LocalIpCore::state(true,true,3,1500,1000),"AUTH_FAILED"));
 assert(!strcmp(LocalIpCore::state(true,false,2,1500,1000),"LINK_DOWN"));
 assert(!strcmp(LocalIpCore::state(false,true,2,1500,1000),"DISABLED"));
 assert(LocalIpCore::fresh(20,0xfffffff0)&&!LocalIpCore::fresh(15000,0xfffffff0));
 assert(LocalIpCore::credential(false,"MASTER_ONLY",false,true));
 assert(!LocalIpCore::credential(false,"DIRECT_ONLY",false,true));
 assert(!LocalIpCore::credential(true,"MASTER_ONLY",false,true));

 using namespace DeliveryCore;
 assert(!eligible(true,false,false,30000,1)); // Slow worker must not multiply copies.
 assert(eligible(false,false,false,30000,1)); // Completion releases retry scheduling.
 assert(!eligible(false,true,true,30000,1)); // Accepted command waits for RESPONSE.
 assert(!eligible(false,true,false,999,1000));assert(eligible(false,true,false,1000,1000));
 assert(eligible(false,false,false,2,0xfffffff0U));
 assert(replaceTelemetry(true,true,false,"DHT11","DHT11"));
 assert(!replaceTelemetry(true,true,true,"DHT11","DHT11")); // Retain in-flight identity/result correlation.
 assert(!replaceTelemetry(true,true,false,"DHT11","INA219"));
 assert(!replaceTelemetry(true,false,false,"DHT11","DHT11")); // Do not coalesce relayed node receipts.
 assert(!replaceTelemetry(false,true,false,"DHT11","DHT11"));
 assert(!replaceTelemetry(true,true,false,"",""));
 assert(transportReceiptCompletes(false,false,false)); // Authenticated HTTP peer owns its queued telemetry.
 assert(transportReceiptCompletes(false,true,false)); // Gateway persistence receipt remains valid.
 assert(!transportReceiptCompletes(true,false,false)); // HTTP command still needs its separate execution response.
 assert(!transportReceiptCompletes(true,true,false));
 assert(!transportReceiptCompletes(false,false,true)); // Radio telemetry still needs the application ACK.
 assert(!applicationReceiptRequired(true,false));
 assert(applicationReceiptRequired(true,true)&&applicationReceiptRequired(false,false));
 puts("PASS production in-flight retry gate, ACK wait, timer wrap, latest sensor coalescing and relayed-message preservation");
}
