#include "node.h"
#include "../core/logger.h"
#include "../core/network_service.h"

NodeRole nodeRole;

void NodeRole::begin() {
    logger.info("ROLE", "NODE role services initialized");
    networkService.begin();
}

void NodeRole::loop() {
    networkService.loop();
}
