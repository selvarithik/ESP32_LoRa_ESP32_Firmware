#include "../src/core/config_storage_core.h"
#include <string>
#include <cassert>
#include <cstdio>
struct Store {
 bool blob=false,legacy=true,writeFailure=false,readFailure=false,corrupt=false,cleanupFailure=false;
 unsigned writes=0;std::string value;
 bool hasBlob(){return blob;}
 bool readBlob(std::string& result){if(readFailure)return false;result=corrupt?"corrupt":value;return true;}
 bool writeBlob(const std::string& next){++writes;if(writeFailure)return false;value=next;blob=true;return true;}
 void eraseLegacy(){if(!cleanupFailure)legacy=false;}
};
int main(){
 const std::string document="canonical document";
 Store good;assert(ConfigStorageCore::persist(good,document));assert(good.blob&&!good.legacy&&good.value==document);assert(ConfigStorageCore::persist(good,document)&&good.writes==1);
 Store failed;failed.writeFailure=true;assert(!ConfigStorageCore::persist(failed,document)&&failed.legacy&&!failed.blob);
 Store corrupt;corrupt.corrupt=true;assert(!ConfigStorageCore::persist(corrupt,document)&&corrupt.legacy);
 Store unreadable;unreadable.blob=true;unreadable.readFailure=true;assert(!ConfigStorageCore::persist(unreadable,document)&&unreadable.legacy&&unreadable.writes==0);
 Store interrupted;interrupted.blob=true;interrupted.value=document;assert(ConfigStorageCore::persist(interrupted,document)&&!interrupted.legacy&&interrupted.writes==0);
 Store cleanup;cleanup.cleanupFailure=true;assert(ConfigStorageCore::persist(cleanup,document)&&cleanup.blob&&cleanup.legacy&&cleanup.value==document);
 puts("PASS config blob migration: verified commit, failed write/read, corruption, interrupted cleanup and idempotency; no physical power-loss test");
}
