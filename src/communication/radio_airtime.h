#pragma once
#include <stddef.h>
#include <stdint.h>
#include "conducted_build.h"

namespace RadioAirtime {
constexpr uint32_t HourMs=3600000;
constexpr uint32_t ProductionLimitUs=36000000;
constexpr size_t MaxUartBytes=200, FixedAddressBytes=3;
constexpr size_t ModuleAllowanceBytes=16;
constexpr uint8_t MaxPollFragments=2, MaxReplyFragments=2;

// Planning model, unverified by EBYTE: use LLCC68's maximum SF (11)
// at 125 kHz as a pessimistic combination, even though the chip does not
// support that pair in practice. CR 4/8, explicit header, PHY CRC on,
// low-data-rate optimization on and an 8-symbol configured preamble.
// Charge actual COBS UART bytes (RV header/CRC, HMAC, payload, delimiter),
// conservatively charge the fixed-address UART prefix if used, and add
// an unverified allowance for opaque module encapsulation.
constexpr uint32_t fragmentUs(size_t uartBytes){
 return uint32_t((49+4*(8+8*((8*(uartBytes+ModuleAllowanceBytes)+35)/36)))*4096);
}
constexpr uint32_t WorstFragmentUs=fragmentUs(MaxUartBytes+FixedAddressBytes);
constexpr uint32_t WorstPollUs=MaxPollFragments*WorstFragmentUs;
constexpr uint32_t WorstReplyUs=MaxReplyFragments*WorstFragmentUs;
// The 200-byte transparent profile's larger typical response costs 12.198 s.
// The ledger remains authoritative for other frame sizes and traffic mixes.
constexpr uint32_t TypicalTurnUs=fragmentUs(200)+fragmentUs(161);
constexpr uint32_t ExchangesPerHour=ProductionLimitUs/TypicalTurnUs;
constexpr uint32_t ProductionPollIntervalMs=(HourMs+ExchangesPerHour-1)/ExchangesPerHour;
#ifdef CONDUCTED_BENCH
constexpr uint32_t LimitUs=600000000, PollIntervalMs=60000, BootQuietMs=15000;
constexpr uint32_t BenchTxGapMs=20000, BenchTxLimit=20;
#else
constexpr uint32_t LimitUs=ProductionLimitUs, PollIntervalMs=ProductionPollIntervalMs, BootQuietMs=HourMs+15000;
#endif
constexpr uint32_t ReplyAssemblyMs=PollIntervalMs*3;
static_assert(ExchangesPerHour>0&&ProductionPollIntervalMs*ExchangesPerHour>=HourMs,"Poll interval exceeds airtime budget");
constexpr bool slotAllows(uint8_t sent,uint8_t cap){return sent<cap;}

class Budget {
 struct Charge {uint32_t expires=0,us=0;};
 static constexpr size_t Capacity=LimitUs/fragmentUs(1)+1;
 Charge records[Capacity];
 size_t first=0,count=0;
 uint32_t total=0;
public:
 uint32_t used(uint32_t now){
  while(count&&int32_t(now-records[first].expires)>=0){
   total-=records[first].us;first=(first+1)%Capacity;--count;
  }
  return total;
 }
 bool debit(uint32_t now,size_t bytes,uint32_t endGuardMs){
  used(now);
  if(!bytes||bytes>MaxUartBytes+FixedAddressBytes)return false;
  const uint32_t us=fragmentUs(bytes);
  if(us>LimitUs-total||count==Capacity)return false;
  auto& item=records[(first+count)%Capacity];
  item.expires=now+endGuardMs+HourMs;item.us=us;
  ++count;total+=us;
  return true;
 }
};
}
