#pragma once
#include "config_schema.h"
// Connection secrets are provisioned over physical USB, never compiled into firmware.
inline void applyTemporaryWiFiDefaults(DeviceConfig&) {}

inline DeviceConfig makeDefaultConfig() {
    DeviceConfig c;
    c.device_name = "ESP32 LoRa Device";
    c.role = DeviceRole::NODE;
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
    c.lora_rx_pin = 16;
    c.lora_tx_pin = 17;
    c.lora_aux_pin = 27;
    c.lora_m0_pin = 25;
    c.lora_m1_pin = 26;
    c.lora_channel = 0;
    c.lora_air_rate = 0;
    c.lora_tx_power = 22;
    c.protocol_version = 1;
    return c;
}
