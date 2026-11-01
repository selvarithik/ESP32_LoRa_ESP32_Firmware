#pragma once
namespace ConfigStorageCore {
// NVS blobs span pages; strings require one contiguous page-sized allocation.
// A legacy record is never removed until the replacement is read back exactly.
template<class Store,class Value> bool persist(Store& store,const Value& value){
 if(store.hasBlob()){
  Value current;if(!store.readBlob(current))return false;
  if(current==value){store.eraseLegacy();return true;}
 }
 if(!store.writeBlob(value))return false;
 Value verified;if(!store.readBlob(verified)||verified!=value)return false;
 store.eraseLegacy(); // Cleanup failure leaves a valid authoritative blob and old fallback.
 return true;
}
}
