#pragma once
#include <Arduino.h>
namespace Commissioning {
bool begin();
void loop();
bool required();
const String& username();
bool verifyCredentials(const String& user,const String& password);
bool updateAccount(const String& user,const String& currentPassword,const String& newPassword);
uint32_t credentialGeneration();
}
