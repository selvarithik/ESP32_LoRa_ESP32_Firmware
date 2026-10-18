#include "master.h"
#include "../core/logger.h"
#include "../core/network_service.h"

MasterRole masterRole;

void MasterRole::begin() {
    logger.info("ROLE", "MASTER role services initialized");
    networkService.begin();
}

void MasterRole::loop() {
    networkService.loop();
}
