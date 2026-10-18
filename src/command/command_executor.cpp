#include "command_executor.h"
#include "../core/logger.h"

CommandExecutor commandExecutor;

void CommandExecutor::begin() {
    logger.info("COMMAND", "Command executor initialized");
}

String CommandExecutor::execute(const String& command, const String& payload) {
    if (command == "PING") return "{\"ok\":true}";
    if (command == "GET_CONFIG") return "{\"ok\":true}";
    if (command == "STATUS") return "{\"ok\":true}";
    if (command == "DIAGNOSTICS") return "{\"ok\":true}";
    return "{\"ok\":false,\"error\":\"UNSUPPORTED_COMMAND\"}";
}
