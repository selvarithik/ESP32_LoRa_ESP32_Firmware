#pragma once
#include "config_schema.h"
#include "hardware_profile.h"
// Connection secrets are provisioned over physical USB, never compiled into firmware.
inline void applyTemporaryWiFiDefaults(DeviceConfig&) {}

inline DeviceConfig makeDefaultConfig() {
    DeviceConfig c;
    c.device_name = "ESP32 LoRa Device";
#ifdef DEVICE_ROLE_MASTER
    c.role = DeviceRole::MASTER;
#else
    c.role = DeviceRole::NODE;
#endif
    c.dht_pin=HardwareProfile::Dht;
    c.tm1637_enabled=false;
    if(HardwareProfile::ExplicitRole){c.dht_enabled=true;c.mq135_enabled=true;c.ina219_enabled=HardwareProfile::Master;}
    c.mesh_id = "MESH-01";
    c.master_id = "";
    c.preferred_master = "";
    c.backup_master = "";
    c.node_address = "1";
    c.allowed_peers_csv = "";
    c.max_hops = 5;
    c.retry_count = 3;
    c.heartbeat_seconds = 30;
    c.route_timeout_seconds = 60;
    c.wifi_enabled = true;
    c.ethernet_enabled = false;
    c.server_host = "";
    c.server_port = 5000;
    c.server_https = false;
    c.lora_baud = 9600;
    c.lora_rx_pin = HardwareProfile::RadioRx;
    c.lora_tx_pin = HardwareProfile::RadioTx;
    c.lora_aux_pin = HardwareProfile::RadioAux;
    c.lora_m0_pin = HardwareProfile::RadioM0;
    c.lora_m1_pin = HardwareProfile::RadioM1;
    c.lora_channel = 0;
    c.lora_air_rate = 0;
    c.lora_tx_power = 22;
    c.protocol_version = 1;
    return c;
}
