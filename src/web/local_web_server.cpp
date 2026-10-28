#include "local_web_server.h"
#include "../core/config_manager.h"
#include "../core/system_manager.h"
#include "../core/network_service.h"
#include "../communication/wifi_manager.h"
#include "../communication/transport_manager.h"
#include "../diagnostics/diagnostics_manager.h"
#include "../core/logger.h"
#include "../core/commissioning.h"
#include <ArduinoJson.h>
#include "mqtt_page.h"
#include "web_assets_gzip.h"
#include "../config/mqtt_staging.h"
#include "../config/mqtt_contract.h"
#include "../config/mqtt_catalog.h"
#include "../core/power_manager.h"
#include "../communication/canonical_mqtt.h"
#include "../config/strict_json.h"

LocalWebServer localWebServer;
namespace {
const char page[] PROGMEM=R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 local gateway</title><style>
body{font:16px/1.5 Arial,sans-serif;background:#f3f7fb;color:#374151;margin:0;padding:24px;max-width:1200px;margin-inline:auto}h1,h2,legend{color:#082b4a}section{background:#fff;border:1px solid #d9e4ef;border-radius:12px;padding:22px;margin-bottom:18px}button,input,select,textarea{font:inherit;min-height:44px;padding:10px;border-radius:7px;max-width:100%;box-sizing:border-box}button{cursor:pointer;margin:5px;background:#0879c9;color:#fff;border:1px solid #0879c9}button:disabled,fieldset:disabled{opacity:.65;cursor:not-allowed}label{display:flex;flex-direction:column;gap:6px;margin:12px 0;overflow-wrap:anywhere}input,select,textarea{background:#fff;color:#082b4a;border:1px solid #a8bdd0}input[type=checkbox]{align-self:flex-start;width:24px;height:24px;min-height:24px}pre{white-space:pre-wrap;overflow-wrap:anywhere}fieldset{border:1px solid #d9e4ef;border-radius:8px;margin:12px 0}legend{font-weight:bold}#feedback{background:#e8f4fc;border:1px solid #a8bdd0;padding:12px;white-space:pre-wrap}small{color:#475569}a{color:#075d9c}nav{display:flex;flex-wrap:wrap;gap:16px;margin-bottom:24px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(230px,100%),1fr));gap:0 20px}:focus-visible{outline:3px solid #0879c9;outline-offset:3px}</style></head><body><h1>ESP32 local gateway</h1><nav><a href="/">Dashboard</a><a href="/network">Network</a><a href="/device">Device</a><a href="/radio">Radio</a><a href="/nodes">Nodes</a><a href="/mqtt">MQTT</a><a href="/account">Account</a><a href="/system">System</a></nav><p>HTTP does not encrypt this local session. Keep administration on a trusted local network.</p><p>Local configuration works without the Gateway. Hardware ID is fixed. Use a logical Node address for assignments.</p><p id="feedback" role="status" aria-live="polite">Loading…</p><section><h2>Device overview</h2><div class="grid"><p>Device <strong id="summaryId">Unknown</strong></p><p>Role <strong id="summaryRole">Unknown</strong></p><p>Station IP <strong id="summaryIp">Unknown</strong></p><p>MQTT <strong id="summaryMqtt">Unknown</strong></p></div><pre id="overview"></pre><button id="refresh">Refresh status</button><button id="reloadSettings" type="button">Read saved settings</button><button id="scan">Scan Wi-Fi</button><pre id="networks"></pre></section><section><h2>Configuration</h2><p>MQ135: keep disabled until a verified divider protects GPIO34 from 5 V AO. INA219 current/power require the onboard shunt value in milliohms (0 = unknown). Sensor changes require restart. Current hardware builds inhibit TM1637; Slave also inhibits INA219. Stored enable flags do not override these build safety limits. Inspect sensor status for effective availability. Do not enable RF until supply, antenna and regional frequency/power settings are confirmed.</p><p>Saved passwords are never returned. Leave secret fields blank to preserve them. Save and apply restarts the board after the response; reconnect at its new IP. Recovery AP returns after a failed Wi-Fi connection.</p><form id="settings"><div id="fields"></div><label>Clear saved Wi-Fi password (physical USB only)<input id="clearWifi" type="checkbox" disabled></label><button type="submit">Save and apply</button></form></section><section id="nodesPanel" hidden><h2>Nodes and routing</h2><p>Last known software registration and routing state. Radio state is reported in diagnostics; physical radio verification is pending.</p><pre id="nodesState"></pre></section><section id="commandsPanel"><h2>Master → Node command</h2><label>Destination device ID<input id="destination" maxlength="48"></label><label>Command<select id="command"><option>PING</option><option>GET_STATUS</option><option>GET_TELEMETRY</option><option>SET_TELEMETRY_INTERVAL</option><option>SET_HEARTBEAT_INTERVAL</option></select></label><label>Interval seconds (interval commands only)<input id="seconds" type="number" min="2" max="3600" value="10"></label><button id="send">Send command</button><pre id="result"></pre></section><section><h2>System</h2><p>Ethernet is unavailable. E220 uses the shared driver and read-only settings check; inspect diagnostics before enabling RF. Internet reachability is UNKNOWN until an optional configured probe completes. OTA is deferred.</p><button id="restart">Restart</button><label>Type the exact device ID to authorize factory reset<input id="resetId" autocomplete="off"></label><button id="reset">Factory reset</button><p>Factory reset requires locally provisioned credentials. Passwords are never printed; use the Account page to change the login password, or physical USB for recovery.</p></section><script>
const $=id=>document.getElementById(id),feedback=t=>$('feedback').textContent=t;
let current={},sessionCsrf='';
const secrets=['wifi_password','admin_password','setup_password','mesh_key','gateway_token','local_ip_token','gateway_ca','internet_probe_ca'];
const groups={Device:['device_name','role','node_address','master_id','enabled','registration_open','allowed_peers_csv','heartbeat_seconds','telemetry_seconds','command_timeout_ms','retry_count','message_max_age_ms','route_timeout_seconds'],WiFi:['wifi_enabled','wifi_ssid','wifi_dhcp','wifi_ip','wifi_subnet','wifi_gateway','wifi_dns','wifi_dns2'],Gateway:['local_ip_enabled','routing_policy','master_host','server_host','server_port','server_https'],Internet:['internet_probe_host','internet_probe_port','internet_probe_tls'],Ethernet:['ethernet_enabled','ethernet_interface','ethernet_dhcp','ethernet_ip','ethernet_subnet','ethernet_gateway','ethernet_dns'],Indicators:['server_led_enabled','server_led_active_high','tm1637_enabled'],Sleep:['sleep_enabled','sleep_start_minute','sleep_duration_seconds','sleep_days_mask','utc_offset_minutes'],DHT11:['dht_enabled','dht_pin'],INA219:['ina219_enabled','ina219_address','ina219_shunt_milliohms'],MQ135:['mq135_enabled','mq135_warmup_seconds'],E220:['lora_enabled','lora_uart','lora_baud','lora_rx_pin','lora_tx_pin','lora_aux_pin','lora_m0_pin','lora_m1_pin','lora_address_high','lora_address_low','lora_channel','lora_air_rate','lora_tx_power','lora_fixed_mode','lora_lbt','lora_rssi'],Advanced:['mesh_id','preferred_master','backup_master','max_hops'],Secrets:secrets};
async function api(path,method='GET',data){if(method!=='GET'&&!sessionCsrf){const session=await (await fetch('/api/auth/session')).json();sessionCsrf=session.csrf;}const r=await fetch(path,{method,headers:{'Content-Type':'application/json','X-Config-Request':'1','X-CSRF-Token':sessionCsrf},body:data===undefined?undefined:JSON.stringify(data)});let d=await r.json();if(r.status===401){location.assign('/login');throw Error('Sign in required');}if(!r.ok)throw Error(d.error||r.statusText);return d;}
async function status(){const d=await api('/api/status');$('overview').textContent=JSON.stringify(d,null,2);$('summaryId').textContent=d.device?.id||'Unknown';$('summaryRole').textContent=d.device?.role||'Unknown';$('summaryIp').textContent=d.wifi?.ip||'Unknown';$('summaryMqtt').textContent=typeof d.mqtt_connected==='boolean'?(d.mqtt_connected?'Connected; check subscription state on MQTT page':'Disconnected'):'Unknown';}
async function load(){current=await api('/api/config');$('fields').replaceChildren();for(const [name,keys] of Object.entries(groups)){let box=document.createElement('fieldset'),legend=document.createElement('legend'),grid=document.createElement('div');legend.textContent=name;box.id='setup_'+name;const unavailable=['Ethernet','E220'].includes(name);if(unavailable){box.disabled=true;legend.textContent+=' · driver unavailable';}box.append(legend);grid.className='grid';box.append(grid);for(const k of keys){let label=document.createElement('label'),el;if(k==='role'){el=document.createElement('select');for(const role of ['MASTER','NODE']){let op=document.createElement('option');op.textContent=role;el.append(op);}}else if(k==='routing_policy'){el=document.createElement('select');for(const v of ['MASTER_ONLY','DIRECT_ONLY','DIRECT_PREFERRED']){const option=document.createElement('option');option.value=v;option.textContent=v;el.append(option);}}else if(k.endsWith('_ca')){el=document.createElement('textarea');el.rows=5;}else{el=document.createElement('input');el.type=secrets.includes(k)?'password':typeof current[k]==='boolean'?'checkbox':typeof current[k]==='number'?'number':'text';}const limits={heartbeat_seconds:[5,3600],telemetry_seconds:[2,3600],retry_count:[0,5],command_timeout_ms:[1000,30000],message_max_age_ms:[1000,300000],max_hops:[1,15],route_timeout_seconds:[10,86400],ina219_address:[64,79],ina219_shunt_milliohms:[0,1000],mq135_warmup_seconds:[60,604800],sleep_start_minute:[0,1439],sleep_duration_seconds:[60,86400],sleep_days_mask:[1,127],utc_offset_minutes:[-720,840],server_port:[1,65535],internet_probe_port:[1,65535]};if(limits[k]){el.min=limits[k][0];el.max=limits[k][1];}if(k==='device_name'){el.required=true;el.maxLength=64;}if(k==='wifi_ssid')el.maxLength=32;el.name=k;el.id='field_'+k;label.textContent=k.replaceAll('_',' ');if(el.type==='checkbox')el.checked=current[k];else el.value=current[k]??'';if(secrets.includes(k)){el.value='';el.autocomplete='new-password';if(!k.endsWith('_ca')){el.disabled=true;el.placeholder='Use physical USB commissioning';}}label.append(el);grid.append(label);} $('fields').append(box);}const selected={network:['WiFi','Gateway','Internet'],device:['Device','Advanced','Indicators','Sleep','DHT11','INA219','MQ135'],radio:['E220'],nodes:[],system:[]}[location.pathname.slice(1)];if(selected)for(const box of $('fields').children)box.hidden=!selected.includes(box.id.slice(6));$('settings').closest('section').hidden=!!selected&&!selected.length;$('send').disabled=current.role!=='MASTER';if(location.pathname==='/nodes'){$('nodesPanel').hidden=false;$('nodesState').textContent=JSON.stringify(await api('/api/mesh'),null,2);}await status();feedback('Ready. Device ID: '+current.device_id+' • firmware '+current.firmware_version);}
$('settings').onsubmit=async e=>{e.preventDefault();try{const data={configuration_revision:current.configuration_revision};if(!confirm('Saving changed connection settings may restart the device. Continue?'))return;for(const el of e.target.elements){if(!el.name||el.disabled||el.closest('fieldset')?.disabled)continue;if(secrets.includes(el.name)&&!el.value)continue;data[el.name]=el.type==='checkbox'?el.checked:el.type==='number'?Number(el.value):el.value;}if($('clearWifi').checked)data.wifi_password='';const result=await api('/api/config','POST',data);feedback(result.restart_required?'Configuration saved. Restarting in 2 seconds. Reconnect and reload.':'Configuration already matches; no restart required.');if(result.restart_required&&Number.isInteger(result.saved_revision))setTimeout(async()=>{try{await load();feedback(current.configuration_revision===result.saved_revision?'Saved state read back after restart.':'Restart pending; read saved settings again after reconnecting.');}catch(error){feedback('Saved; connection interrupted during restart. Reconnect using the actual DHCP address, then read saved settings.');}},5000);}catch(e){feedback(e.message);}};
$('reloadSettings').onclick=()=>load().catch(e=>feedback(e.message));
$('refresh').onclick=()=>status().catch(e=>feedback(e.message));$('scan').onclick=async()=>{try{$('networks').textContent=JSON.stringify(await api('/api/network/scan'),null,2);feedback('Scan starts asynchronously; click again in a few seconds for results.');}catch(e){feedback(e.message);}};
$('send').onclick=async()=>{try{$('result').textContent=JSON.stringify(await api('/api/commands','POST',{destination:$('destination').value,command_type:$('command').value,payload:{seconds:Number($('seconds').value)}}),null,2);feedback('Queued. Refresh status for ACK and final response.');}catch(e){feedback(e.message);}};
$('restart').onclick=async()=>{if(!confirm('Restart the ESP32 and interrupt connections?'))return;try{await api('/api/system/restart','POST',{});feedback('Restart accepted.');}catch(e){feedback(e.message);}};
$('reset').onclick=async()=>{if($('resetId').value!==current.device_id){feedback('Enter the exact device ID first.');return;}if(!confirm('Erase all settings and return to USB commissioning?'))return;try{await api('/api/system/factory-reset','POST',{confirm_device_id:$('resetId').value});feedback('Reset accepted. Use physical USB commissioning to set credentials.');}catch(e){feedback(e.message);}};
load().catch(e=>feedback(e.message));
</script></body></html>)HTML";
}
void LocalWebServer::begin(){const char* headers[]={"Content-Type","X-Config-Request","Authorization","Cookie","X-CSRF-Token"};_server.collectHeaders(headers,5);registerRoutes();_server.begin();}
void LocalWebServer::loop(){_server.handleClient();if(restartAt&&(int32_t)(millis()-restartAt)>=0)systemManager.restart();}
bool LocalWebServer::authorize(bool mutation){
    auto* entry=browserSession();
    if(!entry||!entry->authenticated){if(_server.uri().startsWith("/api/"))sendJson(401,"{\"error\":\"SIGN_IN_REQUIRED\"}");else{_server.sendHeader("Location","/login");_server.send(302,"text/plain","");}return false;}
    bool account=_server.uri()=="/account"||_server.uri()=="/api/account"||_server.uri()=="/api/auth/logout";
    if(Commissioning::required()&&!account){if(_server.uri().startsWith("/api/"))sendJson(403,"{\"error\":\"PASSWORD_CHANGE_REQUIRED\"}");else{_server.sendHeader("Location","/account");_server.send(302,"text/plain","");}return false;}
    if(mutation&&!BrowserSessions::same(entry->csrf,_server.header("X-CSRF-Token").c_str())){sendJson(403,"{\"error\":\"CSRF token required\"}");return false;}
    if(mutation&&restartAt){sendJson(409,"{\"error\":\"Restart pending\"}");return false;}
    return true;
}
bool LocalWebServer::body(size_t maximum){if(_server.header("Content-Type").indexOf("application/json")!=0){sendJson(415,"{\"error\":\"application/json required\"}");return false;}if(!_server.hasArg("plain")||_server.arg("plain").length()>maximum){sendJson(413,"{\"error\":\"Missing or oversized body\"}");return false;}return true;}
void LocalWebServer::registerRoutes(){
    const char* localHeaders[]={"X-Device-ID","X-Local-Nonce","X-Local-Signature","Content-Type","X-CSRF-Token","Cookie","X-Config-Request","Authorization","Transfer-Encoding"};_server.collectHeaders(localHeaders,9);
    _server.on("/api/v1/local/challenge",HTTP_GET,[this](){handleLocalChallenge();});
    _server.on("/api/v1/local/exchange",HTTP_POST,[this](){handleLocalExchange();},[this](){readLocalBody();});
    _server.on("/login",HTTP_GET,[this](){sendAccessPage(false);});
    _server.on("/account",HTTP_GET,[this](){sendAccessPage(true);});
    _server.on("/api/auth/session",HTTP_GET,[this](){handleSession();});
    _server.on("/api/auth/login",HTTP_POST,[this](){handleLogin();});
    _server.on("/api/auth/login/start",HTTP_POST,[this](){handleLoginStart();});
    _server.on("/api/auth/login/result",HTTP_GET,[this](){handleLoginResult();});
    _server.on("/api/auth/logout",HTTP_POST,[this](){handleLogout();});
    _server.on("/api/account",HTTP_POST,[this](){handleAccount();});
    _server.on("/mqtt",HTTP_GET,[this](){if(authorize()){_server.sendHeader("Content-Encoding","gzip");_server.sendHeader("Cache-Control","no-store");_server.send_P(200,"text/html",(const char*)mqttGzip,sizeof(mqttGzip));}});
    _server.on("/api/mqtt",HTTP_GET,[this](){if(authorize()){String status=mqttStaging.status();sendJson(status.indexOf("RESOURCE_LOW")>=0?503:200,status);}});
    _server.on("/api/mqtt/profiles",HTTP_GET,[this](){if(!authorize())return;String raw;int code=MqttCatalog::handle("read","",raw);sendJson(code,raw);});
    for(const char* op:{"save","delete","select"}){String operation=op;_server.on(("/api/mqtt/profiles/"+operation).c_str(),HTTP_POST,[this,operation](){if(!authorize(true)||!body(8192))return;if(configManager.get().role!=DeviceRole::MASTER){sendJson(403,"{\"error\":\"MASTER_REQUIRED\"}");return;}String raw;int code=MqttCatalog::handle(operation,_server.arg("plain"),raw);sendJson(code,raw);});}
    _server.on("/api/power",HTTP_GET,[this](){if(authorize())sendJson(200,powerManager.statusJson());});
    for(const char* op:{"connect","disconnect"}){String operation=op;_server.on(("/api/mqtt/runtime/"+operation).c_str(),HTTP_POST,[this,operation](){
        if(!authorize(true)||!body(256))return;if(configManager.get().role!=DeviceRole::MASTER){sendJson(403,"{\"error\":\"MASTER_REQUIRED\"}");return;}
        JsonDocument input,state;if(!StrictJson::valid(_server.arg("plain"),256)||deserializeJson(input,_server.arg("plain"))||input.size()!=1){sendJson(400,"{\"error\":\"INVALID_REQUEST\"}");return;}deserializeJson(state,mqttStaging.status(false));
        if(!input["expected_revision"].is<uint32_t>()||input["expected_revision"].is<bool>()||input["expected_revision"]!=state["revision"]){sendJson(409,"{\"error\":\"REVISION_CONFLICT\"}");return;}String phase=state["transaction"]["state"]|"UNKNOWN";
        if(phase!="IDLE"&&phase!="PENDING"&&phase!="ROLLED_BACK"&&phase!="COMMITTED"&&phase!="FAILED"){sendJson(409,"{\"error\":\"TRANSACTION_BUSY\"}");return;}
        if(operation=="connect"&&configManager.get().local_ip_enabled){sendJson(409,"{\"error\":\"LOCAL_IP_ONLY_MODE: disable local_ip_enabled and restart before MQTT runtime connect\"}");return;}
        CanonicalMqtt::requestEnabled(operation=="connect");sendJson(202,"{\"requested\":true,\"state\":\"RUNTIME_CHANGE_REQUESTED\"}");
    });}
    for(const char* operation:{"candidate","test","confirm","cancel"}){
        String op=operation;
        _server.on(("/api/mqtt/"+op).c_str(),HTTP_POST,[this,op](){
            if(!authorize(true)||!body(MqttContract::maxRequestBytes))return;
            if(configManager.get().role!=DeviceRole::MASTER){sendJson(403,"{\"ok\":false,\"error\":{\"code\":\"MASTER_REQUIRED\",\"message\":\"MQTT is available only to the Master\"}}");return;}
            String response;int code=mqttStaging.action(op,_server.arg("plain"),response);sendJson(response.indexOf("RESOURCE_LOW")>=0?503:code,response);
        });
    }
    for(const char* path:{"/network","/device","/radio","/nodes","/system"})_server.on(path,HTTP_GET,[this](){if(authorize())handleRoot();});
    _server.on("/",HTTP_GET,[this](){if(authorize())handleRoot();});
    _server.on("/api/status",HTTP_GET,[this](){if(authorize())handleStatus();});
    _server.on("/api/config",HTTP_GET,[this](){if(authorize())handleConfigGet();});
    _server.on("/api/config",HTTP_POST,[this](){if(authorize(true))handleConfigPut();});
    _server.on("/api/network/scan",HTTP_GET,[this](){if(authorize())handleWifiScan();});
    _server.on("/api/commands",HTTP_POST,[this](){if(authorize(true))handleCommand();});
    _server.on("/api/diagnostics",HTTP_GET,[this](){if(authorize())handleDiagnostics();});
    _server.on("/api/mesh",HTTP_GET,[this](){if(authorize())handleMesh();});
    _server.on("/api/system/restart",HTTP_POST,[this](){if(authorize(true))handleRestart();});
    _server.on("/api/system/factory-reset",HTTP_POST,[this](){if(authorize(true))handleFactoryReset();});
    _server.on("/api/v1/packets",HTTP_POST,[this](){handlePacket();});
    _server.onNotFound([this](){sendJson(404,"{\"error\":\"NOT_FOUND\"}");});
}
void LocalWebServer::sendJson(int code,const String& json){_server.sendHeader("Cache-Control","no-store");_server.send(code,"application/json",json);}
void LocalWebServer::handleRoot(){_server.sendHeader("Content-Encoding","gzip");_server.sendHeader("Cache-Control","no-store");_server.send_P(200,"text/html",(const char*)rootGzip,sizeof(rootGzip));}
void LocalWebServer::handleStatus(){sendJson(200,diagnosticsManager.statusJson());}
void LocalWebServer::handleConfigGet(){sendJson(200,configManager.toJson());}
void LocalWebServer::handleConfigPut(){
    if(!body(12288))return;DeviceConfig old=configManager.get();String error;
    JsonDocument candidate;
    if(!StrictJson::valid(_server.arg("plain"),12288)){sendJson(400,"{\"error\":\"Invalid JSON object\"}");return;}
    if(!deserializeJson(candidate,_server.arg("plain")))for(JsonPair item:candidate.as<JsonObject>())if(String(item.key().c_str())=="configuration_revision"&&(!item.value().is<uint32_t>()||item.value().is<bool>()||item.value().as<uint32_t>()!=old.configuration_revision)){sendJson(409,"{\"error\":\"Configuration revision differs; read saved settings before retrying.\"}");return;}
    candidate.clear();
    if(!deserializeJson(candidate,_server.arg("plain")))for(JsonPair entry:candidate.as<JsonObject>()){
        String key=entry.key().c_str();
        if(key=="admin_password"||key=="wifi_password"||key=="setup_password"||key=="mesh_key"||key=="gateway_token"||key=="local_ip_token"){sendJson(403,"{\"error\":\"PROTECTED_PROVISIONING_REQUIRED: use physical USB\"}");return;}
        if(key.startsWith("mqtt_")){
            sendJson(409,"{\"error\":\"STAGED_SWITCH_REQUIRED: use /mqtt; active broker unchanged\"}");return;
        }
    }
    if(!configManager.update(_server.arg("plain"),error)){JsonDocument d;d["error"]=error;String s;serializeJson(d,s);sendJson(400,s);return;}
    // All connection/role changes take effect together after the successful response.
    bool changed=configManager.get().configuration_revision!=old.configuration_revision;
    uint32_t savedRevision=configManager.get().configuration_revision;
    configManager.get()=old;
    JsonDocument result;result["success"]=true;result["saved"]=true;result["restart_required"]=changed;result["saved_revision"]=savedRevision;String raw;serializeJson(result,raw);sendJson(200,raw);
    if(changed)restartAt=millis()+2000;
}
void LocalWebServer::handleWifiScan(){sendJson(200,wifiManager.scanJson());}
void LocalWebServer::handleWifiConnect(){sendJson(410,"{\"error\":\"Use POST /api/config\"}");}
void LocalWebServer::handleCommand(){
    if(!body(2048))return;JsonDocument d;if(deserializeJson(d,_server.arg("plain"))||!d.is<JsonObject>()){sendJson(400,"{\"error\":\"Invalid JSON\"}");return;}
    String args="{}";if(d["payload"].is<JsonObject>()){args="";serializeJson(d["payload"],args);}
    String id=networkService.command(d["destination"]|"",d["command_type"]|"",args);
    if(id.isEmpty()){sendJson(409,"{\"error\":\"Master role, registered destination and queue capacity required\"}");return;}
    sendJson(202,"{\"command_id\":\""+id+"\",\"state\":\"QUEUED\"}");
}
void LocalWebServer::handleDiagnostics(){handleStatus();}
void LocalWebServer::handleMesh(){sendJson(200,networkService.statusJson());}
void LocalWebServer::handleRestart(){sendJson(202,"{\"accepted\":true}");restartAt=millis()+1000;}
void LocalWebServer::handleFactoryReset(){if(!body(256))return;JsonDocument d;if(deserializeJson(d,_server.arg("plain"))||d["confirm_device_id"].as<String>()!=configManager.get().device_id){sendJson(400,"{\"error\":\"Exact confirm_device_id required\"}");return;}if(!configManager.reset()){sendJson(500,"{\"error\":\"NVS reset failed\"}");return;}sendJson(202,"{\"accepted\":true}");restartAt=millis()+1000;}
void LocalWebServer::handlePacket(){
    auto& c=configManager.get();if(c.mesh_key.length()<32||!_server.authenticate("mesh",c.mesh_key.c_str())){sendJson(401,"{\"error\":\"Unauthorized mesh client\"}");return;}
    if(!body(MAX_FRAME_BYTES))return;Message m;if(!Message::fromJson(_server.arg("plain"),m)){sendJson(400,"{\"error\":\"Invalid packet\"}");return;}
    String remote=_server.client().remoteIP().toString();if(!networkService.accept(m,remote)){sendJson(403,"{\"error\":\"Source or destination not authorized\"}");return;}
    if(!transportManager.enqueue(_server.arg("plain"),remote)){sendJson(503,"{\"error\":\"Receive queue full\"}");return;}
    sendJson(202,"{\"accepted\":true}");
}
