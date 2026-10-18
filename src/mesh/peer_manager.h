#pragma once
#include <Arduino.h>

class PeerManager {
public:
    void begin(const String& csv);
    bool isAllowed(const String& peerId) const;
    bool addPeer(const String& peerId);
    bool removePeer(const String& peerId);
    String toJson() const;

private:
    static constexpr size_t MAX_PEERS = 32;
    String _peers[MAX_PEERS];
    size_t _count = 0;
};

extern PeerManager peerManager;
