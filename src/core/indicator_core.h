#pragma once
#include <stdint.h>
#include <stdio.h>
namespace IndicatorCore {
constexpr uint32_t FreshnessMs=15000;
inline bool connected(bool wifi,uint32_t now,uint32_t receipt,uint32_t timeout=FreshnessMs){return wifi&&receipt&&(uint32_t)(now-receipt)<timeout;}
inline uint32_t receiptTimeout(bool master,uint32_t heartbeatSeconds){return master?FreshnessMs:(heartbeatSeconds*2+5)*1000UL;}
inline bool pinConflict(bool master,bool display,bool dht,int dhtPin,bool led,bool radio,int rx,int tx,int aux,int m0,int m1){
 if(master&&display&&dht&&(dhtPin==18||dhtPin==19))return true;
 int pins[]={rx,tx,aux,m0,m1};if(radio)for(int pin:pins)if((led&&pin==2)||(master&&display&&(pin==18||pin==19)))return true;
 return false;
}
struct Frame {uint8_t segments[4]={0,0,0,0};char text[6]={0};};
inline uint8_t digit(unsigned n){const uint8_t table[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};return table[n%10];}
inline Frame screen(bool wifi,bool server,uint32_t now,uint32_t receipt,uint32_t waitingAt,int rssi,uint32_t connectedAt=0){Frame f;
 if(!wifi){uint8_t v[]={0x54,0x5c,0x71,0x06};for(int i=0;i<4;++i)f.segments[i]=v[i];snprintf(f.text,6,"noFI");return f;}
 if(server){unsigned phase=(now/3000)%2;
  if(phase==0){int n=rssi<-999?-999:rssi>0?0:rssi;snprintf(f.text,6,"%4d",n);for(int i=0;i<4;++i)f.segments[i]=f.text[i]==' '?0:f.text[i]=='-'?0x40:digit(f.text[i]-'0');return f;}
  if(phase==1){uint32_t seconds=(uint32_t)(now-connectedAt)/1000;if(seconds>5999)seconds=5999;unsigned minutes=seconds/60,part=seconds%60;snprintf(f.text,6,"%02u:%02u",minutes,part);f.segments[0]=digit(minutes/10);f.segments[1]=digit(minutes%10)|0x80;f.segments[2]=digit(part/10);f.segments[3]=digit(part%10);return f;}
 }
 if(!(now/3000%2)){uint8_t v[]={0x39,0x5c,0x54,0x54};for(int i=0;i<4;++i)f.segments[i]=v[i];snprintf(f.text,6,"Conn");return f;}
 uint32_t seconds=(uint32_t)(now-waitingAt)/1000;if(seconds>999)seconds=999;
 snprintf(f.text,6,"C%03u",(unsigned)seconds);f.segments[0]=0x39;for(int i=1;i<4;++i)f.segments[i]=digit(f.text[i]-'0');return f;
}
}
