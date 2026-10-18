#pragma once
// The same field list drives persistence, validation of JSON types, and public configuration.
#define CONFIG_FIELDS(X) \
 X(device_name) X(mesh_id) X(master_id) X(preferred_master) X(backup_master) \
 X(node_address) X(allowed_peers_csv) X(max_hops) X(retry_count) X(heartbeat_seconds) \
 X(route_timeout_seconds) X(wifi_enabled) X(wifi_ssid) X(wifi_password) \
 X(ethernet_enabled) X(ble_enabled) X(server_host) X(server_port) X(server_https) \
 X(lora_baud) X(lora_rx_pin) X(lora_tx_pin) X(lora_aux_pin) X(lora_m0_pin) X(lora_m1_pin) \
 X(lora_channel) X(lora_air_rate) X(lora_tx_power) X(enabled) X(registration_open) \
 X(admin_password) X(setup_password) X(mesh_key) X(master_host) X(gateway_token) X(gateway_ca) \
 X(telemetry_seconds) X(command_timeout_ms) X(message_max_age_ms) X(dht_enabled) X(dht_pin) \
 X(wifi_dhcp) X(wifi_ip) X(wifi_subnet) X(wifi_gateway) X(wifi_dns) \
 X(ethernet_interface) X(ethernet_dhcp) X(ethernet_ip) X(ethernet_subnet) X(ethernet_gateway) X(ethernet_dns) \
 X(ble_name) X(lora_enabled) X(lora_uart) X(lora_address_high) X(lora_address_low) \
 X(lora_fixed_mode) X(lora_lbt) X(lora_rssi) X(mqtt_enabled) X(mqtt_tls) X(mqtt_host) \
 X(mqtt_user) X(mqtt_password) X(mqtt_ca) X(mqtt_port) X(mqtt_keepalive) \
 X(internet_probe_host) X(internet_probe_ca) X(internet_probe_port) X(internet_probe_tls)
