#pragma once
#include "../drivers/hardware_interfaces.h"
#include "dht_pulse_core.h"
#include <DHT.h>
#include <cmath>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include <freertos/ringbuf.h>
class DhtProvider:public Hardware::SensorProvider {
 int pin;DHT fallback;RingbufHandle_t ring=nullptr;bool ready=false,pollNext=false;
public:
 DhtPulseCore::Result result=DhtPulseCore::Result::Driver;uint16_t pulses=0;bool idleHigh=false,usedPolling=false;
 explicit DhtProvider(int gpio):pin(gpio),fallback(gpio,DHT11){}
 void begin(){
  fallback.begin();
  rmt_config_t c{};c.rmt_mode=RMT_MODE_RX;c.channel=RMT_CHANNEL_0;c.gpio_num=(gpio_num_t)pin;c.clk_div=80;c.mem_block_num=1;c.rx_config.filter_en=true;c.rx_config.filter_ticks_thresh=3;c.rx_config.idle_threshold=2000;
  if(rmt_config(&c)!=ESP_OK||rmt_driver_install(RMT_CHANNEL_0,1024,0)!=ESP_OK)return;
  ready=rmt_get_ringbuf_handle(RMT_CHANNEL_0,&ring)==ESP_OK&&ring;
 }
 Hardware::SensorSample read()override{
  Hardware::SensorSample value;pulses=0;result=DhtPulseCore::Result::Driver;usedPolling=pollNext||!ready;
  if(usedPolling){
   idleHigh=gpio_get_level((gpio_num_t)pin);value.humidity=fallback.readHumidity();value.temperature=fallback.readTemperature();
   value.valid=std::isfinite(value.temperature)&&std::isfinite(value.humidity)&&value.temperature>=0&&value.temperature<=50&&value.humidity>=0&&value.humidity<=100;
   result=value.valid?DhtPulseCore::Result::Ok:DhtPulseCore::Result::PollingRead;pollNext=value.valid;return value;
  }
  // Restore RX routing after a polling attempt changes the GPIO configuration.
  if(rmt_set_gpio(RMT_CHANNEL_0,RMT_MODE_RX,(gpio_num_t)pin,false)!=ESP_OK)return value;
  size_t length=0;while(void* old=xRingbufferReceive(ring,&length,0))vRingbufferReturnItem(ring,old);
  gpio_set_direction((gpio_num_t)pin,GPIO_MODE_INPUT_OUTPUT_OD);gpio_set_pull_mode((gpio_num_t)pin,GPIO_PULLUP_ONLY);gpio_set_level((gpio_num_t)pin,1);idleHigh=gpio_get_level((gpio_num_t)pin);
  gpio_set_level((gpio_num_t)pin,0);vTaskDelay(pdMS_TO_TICKS(20));
  if(rmt_rx_start(RMT_CHANNEL_0,true)!=ESP_OK){gpio_set_level((gpio_num_t)pin,1);return value;}
  gpio_set_level((gpio_num_t)pin,1);
  auto* items=(rmt_item32_t*)xRingbufferReceive(ring,&length,pdMS_TO_TICKS(30));rmt_rx_stop(RMT_CHANNEL_0);
  if(!items){result=DhtPulseCore::Result::NoResponse;pollNext=true;return value;}
  DhtPulseCore::Pulse captured[128]{};
  for(size_t i=0;i<length/sizeof(*items)&&pulses<126;++i){if(items[i].duration0)captured[pulses++]={uint16_t(items[i].duration0),bool(items[i].level0)};if(items[i].duration1)captured[pulses++]={uint16_t(items[i].duration1),bool(items[i].level1)};}
  vRingbufferReturnItem(ring,items);result=DhtPulseCore::decode(captured,pulses,value.temperature,value.humidity);value.valid=result==DhtPulseCore::Result::Ok;pollNext=!value.valid;return value;
 }
};
