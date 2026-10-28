#pragma once
#include <Arduino.h>

enum class DeviceRole : uint8_t {
    MASTER = 0,
    NODE = 1
};

struct DeviceConfig {
    String device_id;
    String device_name;
    DeviceRole role = DeviceRole::NODE;

    String mesh_id;
    String master_id;
    String preferred_master;
    String backup_master;
    String node_address;
    String allowed_peers_csv;

    uint8_t max_hops = 5;
    uint8_t retry_count = 3;
    uint32_t heartbeat_seconds = 30;
    uint32_t route_timeout_seconds = 60;

    bool wifi_enabled = true;
    String wifi_ssid;
    String wifi_password;

    bool ethernet_enabled = false;

    String server_host;
    uint16_t server_port = 5000;
    bool server_https = false;
    bool local_ip_enabled = false;
    String local_ip_token;
    String routing_policy = "MASTER_ONLY";

    uint32_t lora_baud = 9600;
    int lora_rx_pin = 16;
    int lora_tx_pin = 17;
    int lora_aux_pin = 27;
    int lora_m0_pin = 25;
    int lora_m1_pin = 26;

    uint8_t lora_channel = 0;
    uint8_t lora_air_rate = 0;
    uint8_t lora_tx_power = 22;

    uint8_t protocol_version = 1;

    uint32_t configuration_version = 1;
    uint32_t configuration_revision = 1;
    bool enabled = true;
    bool registration_open = false;
    String admin_password;
    String setup_password;
    String mesh_key;
    String master_host;
    String gateway_token;
    String gateway_ca;
    uint32_t telemetry_seconds = 10;
    uint32_t command_timeout_ms = 5000;
    uint32_t message_max_age_ms = 60000;
    bool dht_enabled = false;
    int dht_pin = 4;
    bool wifi_dhcp = true;
    String wifi_ip, wifi_subnet = "255.255.255.0", wifi_gateway, wifi_dns, wifi_dns2;
    bool server_led_enabled = true;
    bool server_led_active_high = true;
    bool tm1637_enabled = false;
    bool ina219_enabled = true;
    uint8_t ina219_address = 64;
    uint16_t ina219_shunt_milliohms = 0; // Unknown: no current/power until configured.
    bool mq135_enabled = false; // Physical voltage-divider verification required.
    uint32_t mq135_warmup_seconds = 86400; // Conservative initial conditioning, not calibration.
    bool sleep_enabled = false;
    uint16_t sleep_start_minute = 0;
    uint32_t sleep_duration_seconds = 3600;
    uint8_t sleep_days_mask = 127;
    int utc_offset_minutes = 330;
    String ethernet_interface = "NOT_CONFIGURED";
    bool ethernet_dhcp = true;
    String ethernet_ip, ethernet_subnet, ethernet_gateway, ethernet_dns;
    bool lora_enabled = false;
    uint8_t lora_uart = 2;
    uint8_t lora_address_high = 0, lora_address_low = 0;
    bool lora_fixed_mode = false, lora_lbt = false, lora_rssi = false;
    bool mqtt_enabled = false, mqtt_tls = false;
    String mqtt_host, mqtt_user, mqtt_password, mqtt_ca;
    uint16_t mqtt_port = 1883;
    uint16_t mqtt_keepalive = 30;
    String internet_probe_host, internet_probe_ca;
    uint16_t internet_probe_port = 443;
    bool internet_probe_tls = true;
};
