#pragma once
#include <cstring>
namespace CommissioningRoleCore {
inline bool confirmed(bool explicitBuild,const char* identity,const char* confirmation){
 return explicitBuild&&identity&&confirmation&&identity[0]&&std::strlen(identity)<=48&&std::strcmp(identity,confirmation)==0;
}
inline bool clearSelfAssignment(bool targetMaster,const char* identity,const char* assignedMaster){
 return !targetMaster&&identity&&assignedMaster&&std::strcmp(identity,assignedMaster)==0;
}
}
