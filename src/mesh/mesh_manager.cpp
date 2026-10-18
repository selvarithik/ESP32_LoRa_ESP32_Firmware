#include "mesh_manager.h"
#include "peer_manager.h"
#include "../core/logger.h"
#include <ArduinoJson.h>

MeshManager meshManager;

void MeshManager::begin(const DeviceConfig& config) {
    _deviceId = config.device_id;
    _meshId = config.mesh_id;
    _masterId = config.master_id;
    _preferredMaster = config.preferred_master;
    _backupMaster = config.backup_master;
    _nodeAddress = config.node_address;
    _maxHops = config.max_hops;

    peerManager.begin(config.allowed_peers_csv);

    logger.info("MESH", "Mesh foundation initialized; peers=" + peerManager.toJson());
}

String MeshManager::selectNextHop(const String& destination) const {
    if (destination == _deviceId) return _deviceId;
    if (peerManager.isAllowed(destination)) return destination;
    if (_preferredMaster.length() && peerManager.isAllowed(_preferredMaster)) return _preferredMaster;
    if (_backupMaster.length() && peerManager.isAllowed(_backupMaster)) return _backupMaster;
    if (_masterId.length() && peerManager.isAllowed(_masterId)) return _masterId;
    return "";
}

bool MeshManager::isPeerAllowed(const String& peer) const {
    return peerManager.isAllowed(peer);
}

String MeshManager::statusJson() const {
    JsonDocument doc;
    doc["mesh_id"] = _meshId;
    doc["device_id"] = _deviceId;
    doc["master_id"] = _masterId;
    doc["preferred_master"] = _preferredMaster;
    doc["backup_master"] = _backupMaster;
    doc["node_address"] = _nodeAddress;
    doc["max_hops"] = _maxHops;
    doc["allowed_peers"] = serialized(peerManager.toJson());

    String out;
    serializeJson(doc, out);
    return out;
}

void MeshManager::loop() {
    // Full neighbor scoring and multi-hop route maintenance are Phase 2.
}
