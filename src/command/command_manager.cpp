#include "command_manager.h"
#include "../core/network_service.h"
#include "../core/config_manager.h"
CommandManager commandManager;
void CommandManager::begin(){}
void CommandManager::loop(){} // Receive ownership belongs exclusively to NetworkService.
bool CommandManager::processIncoming(const Message&){return false;}
String CommandManager::createLocalCommand(const String& type,const String& payload){return networkService.command(configManager.get().master_id,type,payload);}
String CommandManager::stateJson()const{return networkService.statusJson();}
