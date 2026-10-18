#include "peer_manager.h"
#include <ArduinoJson.h>

PeerManager peerManager;

void PeerManager::begin(const String& csv) {
    _count = 0;

    int start = 0;
    while (start < csv.length() && _count < MAX_PEERS) {
        int comma = csv.indexOf(',', start);
        if (comma < 0) comma = csv.length();

        String item = csv.substring(start, comma);
        item.trim();

        if (item.length()) {
            bool duplicate = false;
            for (size_t i = 0; i < _count; ++i) {
                if (_peers[i] == item) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                _peers[_count++] = item;
            }
        }

        start = comma + 1;
    }
}

bool PeerManager::isAllowed(const String& peerId) const {
    for (size_t i = 0; i < _count; ++i) {
        if (_peers[i] == peerId) return true;
    }
    return false;
}

bool PeerManager::addPeer(const String& peerId) {
    String p = peerId;
    p.trim();
    if (!p.length() || _count >= MAX_PEERS || isAllowed(p)) return false;
    _peers[_count++] = p;
    return true;
}

bool PeerManager::removePeer(const String& peerId) {
    for (size_t i = 0; i < _count; ++i) {
        if (_peers[i] == peerId) {
            for (size_t j = i + 1; j < _count; ++j) {
                _peers[j - 1] = _peers[j];
            }
            --_count;
            return true;
        }
    }
    return false;
}

String PeerManager::toJson() const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < _count; ++i) {
        arr.add(_peers[i]);
    }

    String out;
    serializeJson(arr, out);
    return out;
}
