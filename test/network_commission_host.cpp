#include "../src/core/network_commission_core.h"
#include "../src/core/commissioning_role_core.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
int main(){
 using namespace NetworkCommissionCore;
 assert(allowed("wifi_ssid")&&allowed("wifi_password")&&allowed("master_host")&&allowed("master_id")&&allowed("mesh_key"));
 assert(allowed("local_ip_token")&&allowed("local_ip_enabled")&&allowed("allowed_peers_csv"));
 for(const char* field:{"role","device_id","mqtt_password","mqtt_host","mqtt_enabled","profile","dht_pin","dht_enabled","ina219_enabled","mq135_enabled","admin_password","setup_password","gateway_token","sleep_enabled","wifi_password_extra",""})assert(!allowed(field));
 assert(!allowed(nullptr)&&!count(0)&&count(1)&&count(13)&&!count(14));
 assert(CommissioningRoleCore::confirmed(true,"SLAVE-ID","SLAVE-ID"));
 assert(!CommissioningRoleCore::confirmed(true,"SLAVE-ID","OTHER-ID"));
 assert(!CommissioningRoleCore::confirmed(false,"SLAVE-ID","SLAVE-ID"));
 puts("PASS production USB network allowlist, bounds, foreign identity and protected MQTT/sensor/account rejection");
}
