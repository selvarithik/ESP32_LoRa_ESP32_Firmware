// Included inside NetworkService namespace; single main-loop owner.
struct OperationJob {bool active=false,gateway=false,receipt=false,acceptance=false,performed=false;String type,host,probe;Message request;uint32_t started=0,timeout=90000,lastPoll=0,baselineRecycles=0;bool oldMqtt=false;};
OperationJob operationJob;uint32_t mqttRestoreAt=0;bool mqttRestoreEnabled=false,mqttRestoreArmed=false;

bool operationCoordinatorBusy(){if(operationJob.active||mqttRestoreArmed||TransportSwitch::busy())return true;JsonDocument d;if(deserializeJson(d,mqttStaging.status(false)))return true;String state=d["transaction"]["state"]|"UNKNOWN";return state!="IDLE"&&state!="CANCELLED"&&state!="ROLLED_BACK"&&state!="COMMITTED"&&state!="FAILED";}
bool operationError(String& result,const char* error){JsonDocument d;d["error"]=error;serializeJson(d,result);return false;}
bool startOperation(const Message& m,const String& type,JsonObjectConst args,String& result,bool& deferred,bool gateway,const String& channel){
 deferred=false;if(!gateway||m.source!="GATEWAY")return operationError(result,"GATEWAY_AUTHORIZATION_REQUIRED");
 if(type=="GET_COMMAND_RESULT"){String cid=args["command_id"]|"";JsonDocument out;out["command_id"]=cid;out["scope"]="VOLATILE_CACHE";for(auto& entry:cache)if(entry.source==m.source&&entry.id==cid&&cid!=m.messageId){out["known"]=true;out["state"]=entry.executed?(entry.success?"COMPLETED":"FAILED"):"EXECUTING_OR_UNKNOWN";out["result_retained"]=entry.response.length()>0;if(entry.response.length()&&entry.response.length()<=1500){JsonDocument response;if(!deserializeJson(response,entry.response))out["data"]=response["payload"]; }serializeJson(out,result);return true;}out["known"]=false;out["state"]="UNKNOWN_OR_EXPIRED";serializeJson(out,result);return true;}
 if(type=="SYNC_TIME"){if(operationCoordinatorBusy()||powerManager.draining())return operationError(result,"COORDINATOR_OR_POWER_BUSY");int64_t target=args["unix_time"].as<uint32_t>(),now=time(nullptr);if((m.timestamp>target+30)||(target-m.timestamp>60))return operationError(result,"TIME_ENVELOPE_MISMATCH");if(now>=1700000000&&llabs(target-now)>300)return operationError(result,"CLOCK_STEP_REQUIRES_LOCAL_COMMISSIONING");timeval tv={(time_t)target,0};if(settimeofday(&tv,nullptr))return operationError(result,"TIME_SET_FAILED");JsonDocument out;out["unix_time"]=time(nullptr);out["source"]="AUTHENTICATED_GATEWAY";out["configuration_changed"]=false;serializeJson(out,result);return true;}
 if(type=="RESET_DIAGNOSTIC_COUNTERS"){JsonDocument out;out["scope"]="NETWORK";out["previous_delivery_failures"]=failed;out["previous_duplicates"]=duplicates;failed=0;duplicates=0;out["reset"]=true;out["queue_preserved"]=true;out["history_preserved"]=true;serializeJson(out,result);return true;}
 if(type=="CANCEL_PENDING_SLEEP"){String error;if(!powerManager.cancelPendingSleep(error))return operationError(result,error.c_str());result="{\"cancelled\":true,\"saved_schedule_changed\":false}";return true;}
 if(type=="SLEEP_NOW"){if(operationJob.active||mqttRestoreArmed)return operationError(result,"OPERATION_BUSY");String error;if(!powerManager.requestSleep(args["duration_seconds"],args["mode"].as<String>(),error))return operationError(result,error.c_str());result="{\"state\":\"PREPARING_TO_SLEEP\",\"request_applied\":true,\"sleep_entry_confirmed\":false,\"saved_schedule_changed\":false,\"remote_wake_supported\":false}";return true;}
 if(operationJob.active||powerManager.draining()||operationCoordinatorBusy())return operationError(result,"OPERATION_BUSY");
 OperationJob job;job.active=true;job.request=m;job.type=type;job.host=channel;job.gateway=gateway;job.started=millis();
 if(type=="WIFI_RECONNECT"){
  if(configManager.get().wifi_ssid.isEmpty())return operationError(result,"SSID_REQUIRED");
  JsonDocument status;if(deserializeJson(status,CanonicalMqtt::status()))return operationError(result,"MQTT_STATE_UNKNOWN");
  job.oldMqtt=status["runtime_enabled"]|false;job.baselineRecycles=status["tls_client_recycles"]|0;
  operationJob=job;deferred=true;return true;
 }
 if(type.startsWith("MQTT_")){if(configManager.get().role!=DeviceRole::MASTER)return operationError(result,"MASTER_ONLY");JsonDocument status;if(deserializeJson(status,CanonicalMqtt::status())||!status["runtime_enabled"].is<bool>())return operationError(result,"MQTT_STATE_UNKNOWN");job.oldMqtt=status["runtime_enabled"].as<bool>();job.baselineRecycles=status["tls_client_recycles"]|0;if(type=="MQTT_TEST"){if(!CanonicalMqtt::connected()||status["subscription_state"]!="READY")return operationError(result,"MQTT_NOT_READY");Message probe=make(PacketType::STATUS,"GATEWAY",bodyStatus());job.probe=probe.messageId;if(!CanonicalMqtt::publish(probe))return operationError(result,"MQTT_QUEUE_BUSY");job.timeout=15000;}else{if(type!="MQTT_CONNECT"&&(channel!="IP"||!transportManager.localMode()||!transportManager.gatewayConnected()))return operationError(result,"VERIFIED_IP_RETURN_PATH_REQUIRED");if(type=="MQTT_DISCONNECT"){if(mqttRestoreArmed)return operationError(result,"MQTT_RECOVERY_PENDING");mqttRestoreEnabled=job.oldMqtt;mqttRestoreArmed=true;mqttRestoreAt=millis()+args["restore_after_seconds"].as<uint32_t>()*1000;CanonicalMqtt::requestEnabled(false);}else{CanonicalMqtt::requestEnabled(true);if(type=="MQTT_RECONNECT")CanonicalMqtt::requestReconnect();}}}
 else if(type=="RADIO_INSPECT"){job.timeout=15000;if(loraTransport.isInitialized()){JsonDocument current;deserializeJson(current,loraTransport.statusJson());current["readback_source"]="CACHED_DRIVER_SETTINGS";current["fresh_probe"]=false;serializeJson(current,result);return true;}if(!loraTransport.requestProbe(true))return operationError(result,"RADIO_BUSY_OR_UNAVAILABLE");}
 else if(type=="DISCOVER_NODES"){if(!configManager.get().registration_open)return operationError(result,"REGISTRATION_CLOSED");if(!configManager.get().lora_enabled||!loraTransport.isInitialized())return operationError(result,"RADIO_NOT_READY_OR_NOT_COMMISSIONED");job.timeout=args["timeout_seconds"].as<uint32_t>()*1000;if(!track(make(PacketType::HELLO,"BROADCAST",bodyStatus()),"RADIO:BROADCAST"))return operationError(result,"DISCOVERY_QUEUE_FULL");}
 else if(type=="I2C_SCAN"){job.timeout=15000;if(!sensorManager.requestI2cScan())return operationError(result,"I2C_BUS_UNAVAILABLE_OR_BUSY");}
 else return operationError(result,"UNSUPPORTED_OPERATION");
 operationJob=job;deferred=true;return true;
}
void finishOperation(bool ok,JsonDocument& data){auto& job=operationJob;JsonDocument response;response["command_id"]=job.request.messageId;response["status"]=ok?"SUCCESS":"FAILED";response["execution_duration_ms"]=millis()-job.started;response["data"]=data;String raw;serializeJson(response,raw);Message out=make(PacketType::RESPONSE,job.request.source,raw);out.correlationId=job.request.messageId;
 if(raw.length()>MAX_PAYLOAD_BYTES||out.toJson().length()>MAX_FRAME_BYTES){out.payload="{\"command_id\":\""+job.request.messageId+"\",\"status\":\"FAILED\",\"data\":{\"error\":\"RESULT_TOO_LARGE\"}}";ok=false;}
 for(auto& entry:cache)if(entry.source==job.request.source&&entry.id==job.request.messageId){entry.response=out.toJson();entry.responseId=out.messageId;entry.executed=true;entry.success=ok;entry.seen=millis();}
 track(out,job.gateway?configManager.get().server_host:job.host,job.gateway);latestCommand=job.request.messageId;latestState=ok?"COMPLETED":"FAILED";operationJob=OperationJob{};
}
void pollOperation(){uint32_t now=millis();if(mqttRestoreArmed&&(int32_t)(now-mqttRestoreAt)>=0){CanonicalMqtt::requestEnabled(mqttRestoreEnabled);mqttRestoreAt=0;mqttRestoreArmed=false;}auto& job=operationJob;if(!job.active||now-job.lastPoll<100)return;job.lastPoll=now;JsonDocument out;bool done=false,ok=false;
 if(job.type=="WIFI_RECONNECT"){
  if(!job.performed&&job.acceptance){
   auto& c=configManager.get();if(!wifiManager.connect(c.wifi_ssid,c.wifi_password,0)){done=true;out["error"]="SSID_REQUIRED";}
   else {job.performed=true;if(job.oldMqtt)CanonicalMqtt::requestReconnect();}
  }
  if(!job.performed&&now-job.started>=5000){done=true;out["error"]="ACCEPTANCE_RECEIPT_TIMEOUT_NO_RECONNECT";}
  if(job.performed){JsonDocument status;deserializeJson(status,CanonicalMqtt::status());
   bool mqttReady=!job.oldMqtt||(status["state"]=="connected"&&status["subscription_state"]=="READY"&&status["tls_client_recycles"].as<uint32_t>()>job.baselineRecycles);
   if(wifiManager.isConnected()&&mqttReady){done=ok=true;out["wifi_reconnected"]=true;out["mqtt_ready"]=job.oldMqtt;out["saved_configuration_changed"]=false;}
  }
 }
 else if(job.type=="MQTT_TEST"&&job.receipt){done=ok=true;out["gateway_persisted_receipt"]=true;out["probe_message_id"]=job.probe;out["transport"]="MQTT";}
 else if(job.type.startsWith("MQTT_")&&job.type!="MQTT_TEST"){deserializeJson(out,CanonicalMqtt::status());done=job.type=="MQTT_DISCONNECT"?out["state"]=="paused"&&!CanonicalMqtt::connected():out["state"]=="connected"&&out["subscription_state"]=="READY";if(job.type=="MQTT_RECONNECT"&&out["tls_client_recycles"].as<uint32_t>()<=job.baselineRecycles)done=false;ok=done;if(done&&job.type=="MQTT_DISCONNECT")out["automatic_restore_armed"]=true;}
 else if(job.type=="RADIO_INSPECT"){deserializeJson(out,loraTransport.statusJson());if(out["state"]=="ERROR"){done=true;ok=out["public_settings_read"]==true&&out["error"]=="PROBE_COMPLETE_NO_RF";}out["rf_transmission"]=false;out["register_write_requested"]=false;}
 else if(job.type=="I2C_SCAN"){deserializeJson(out,sensorManager.i2cScanJson());done=out["state"]=="COMPLETED";ok=done;}
 else if(job.type=="DISCOVER_NODES"&&OperationRules::expired(now,job.started,job.timeout)){unsigned count=0;auto list=out["responding_nodes"].to<JsonArray>();for(auto& peer:peers)if(peer.id.length()&&peer.host.startsWith("RADIO:")&&(int32_t)(peer.seen-job.started)>=0){list.add(peer.id);++count;}out["rf_delivery_verified"]=count>0;out["node_count"]=count;done=true;ok=count>0;if(!ok)out["error"]="NO_SLAVE_RESPONSE";}
 if(!done&&OperationRules::expired(now,job.started,job.timeout)){done=true;out["error"]="OPERATION_TIMEOUT";out["outcome"]="UNKNOWN";}
 if(done){if(!ok&&job.type.startsWith("MQTT_")&&job.type!="MQTT_TEST"){CanonicalMqtt::requestEnabled(job.oldMqtt);out["runtime_restore_requested"]=true;}finishOperation(ok,out);}
}
