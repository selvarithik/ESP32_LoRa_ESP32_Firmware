#include "../src/core/commissioning_role_core.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace CommissioningRoleCore;
 assert(confirmed(true,"ESP32-A","ESP32-A"));
 assert(!confirmed(false,"ESP32-A","ESP32-A"));
 assert(!confirmed(true,"ESP32-A","ESP32-B"));
 assert(!confirmed(true,"",""));assert(!confirmed(true,nullptr,"ESP32-A"));
 assert(clearSelfAssignment(false,"ESP32-A","ESP32-A"));
 assert(!clearSelfAssignment(true,"ESP32-A","ESP32-A"));
 assert(!clearSelfAssignment(false,"ESP32-A","ESP32-PARENT"));
 puts("PASS physical build-role confirmation and self-assignment repair; legitimate parent retained");
}
