#include "command_executor.h"
#include "../core/logger.h"

CommandExecutor commandExecutor;

void CommandExecutor::begin() {
    logger.info("COMMAND", "Command executor initialized");
}

String CommandExecutor::execute(const String& command, const String& payload) {
    (void)command;(void)payload;
    return "{\"ok\":false,\"error\":\"USE_NETWORK_SERVICE_DISPATCHER\"}";
}
