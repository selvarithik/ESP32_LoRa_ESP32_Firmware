// Browser checks use simulated API replies; they do not claim on-board verification.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'C:/Users/Admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'src/web/local_web_server.cpp'),'utf8');
const html=source.match(/R"HTML\(([\s\S]*?)\)HTML"/)[1];
const config={device_id:'ESP32-TEST00000001',device_name:'Bench master',role:'MASTER',firmware_version:'1.0.0-part1',configuration_revision:2};
for(const k of ['enabled','registration_open','wifi_enabled','wifi_dhcp','ethernet_enabled','ethernet_dhcp','ble_enabled','dht_enabled','lora_enabled','lora_fixed_mode','lora_lbt','lora_rssi','mqtt_enabled','mqtt_tls','server_https'])config[k]=!['mqtt_enabled','mqtt_tls','server_https','ethernet_enabled','ble_enabled','dht_enabled','lora_enabled'].includes(k);
Object.assign(config,{node_address:'1',master_id:config.device_id,heartbeat_seconds:30,telemetry_seconds:10,command_timeout_ms:5000,retry_count:3,message_max_age_ms:60000,route_timeout_seconds:60,server_port:5000,mqtt_port:1883,mqtt_keepalive:30,dht_pin:4,lora_uart:2,lora_baud:9600,lora_rx_pin:16,lora_tx_pin:17,lora_aux_pin:27,lora_m0_pin:25,lora_m1_pin:26,lora_address_high:0,lora_address_low:0,lora_channel:0,lora_air_rate:0,lora_tx_power:22,max_hops:5,internet_probe_host:'',internet_probe_port:443,internet_probe_tls:true});
(async()=>{
 const browser=await chromium.launch({channel:'msedge',headless:true});
 try{
  for(const width of [1280,375]){
   const ctx=await browser.newContext({viewport:{width,height:900}}),page=await ctx.newPage();let invalid=false,unchanged=false,lastSave=null,lastReset=null;
   const errors=[];page.on('pageerror',e=>errors.push(e.message));
   await page.route('http://esp32.test/**',async route=>{
    const req=route.request(),url=new URL(req.url());let status=200,body={};
    if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
    if(url.pathname==='/api/auth/session')body={csrf:'fixture-csrf',authenticated:true};
    else if(url.pathname==='/api/config'&&req.method()==='GET')body=config;
    else if(url.pathname==='/api/status')body={device:{id:config.device_id,role:'MASTER'},sensor:{status:'NOT_CONFIGURED'},ethernet_status:'NOT_AVAILABLE',network:{command_state:'COMPLETED'}};
    else if(url.pathname==='/api/config'){lastSave=req.postDataJSON();assert.equal(req.headers()['x-config-request'],'1');if(invalid){status=400;body={error:'Intervals out of range'};}else body={saved:true,restart_required:!unchanged};}
    else if(url.pathname==='/api/commands')body={command_id:'MSG-TEST',state:'QUEUED'};
    else if(url.pathname==='/api/system/factory-reset'){lastReset=req.postDataJSON();body={accepted:true};}
    else if(url.pathname==='/api/network/scan')body=[];
    else body={accepted:true};
    await route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
   });
   await page.goto('http://esp32.test/');
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));
   assert.equal(await page.locator('#field_admin_password').inputValue(),'');
   assert.equal(await page.locator('#field_gateway_ca').evaluate(e=>e.tagName),'TEXTAREA');
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
   await page.locator('#field_telemetry_seconds').fill('15');
   page.once('dialog',d=>d.accept());invalid=true;await page.getByRole('button',{name:'Save and apply',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent==='Intervals out of range');
   assert.equal(lastSave.telemetry_seconds,15);assert.equal(typeof lastSave.enabled,'boolean');assert.ok(!('admin_password' in lastSave));
   page.once('dialog',d=>d.accept());invalid=false;unchanged=true;await page.getByRole('button',{name:'Save and apply',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.includes('no restart required'));
   await page.locator('#destination').fill('ESP32-NODE');await page.getByRole('button',{name:'Send command',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('result').textContent.includes('QUEUED'));
   await page.getByRole('button',{name:'Factory reset',exact:true}).click();assert.equal(lastReset,null);
   await page.locator('#resetId').fill(config.device_id);page.once('dialog',d=>d.accept());await page.getByRole('button',{name:'Factory reset',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Reset accepted'));
   assert.deepEqual(lastReset,{confirm_device_id:config.device_id});assert.deepEqual(errors,[]);
   if(process.env.CAPTURE_SCREENSHOTS==='1')await page.screenshot({path:path.join(root,'docs',`local-page-mock-${width}.png`),fullPage:true});
   console.log(`PASS local page ${width}px: fields, types, secrets, validation, save, commands, reset, overflow`);
   await ctx.close();
  }
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
