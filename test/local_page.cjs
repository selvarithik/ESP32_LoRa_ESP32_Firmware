// Browser checks use simulated API replies; they do not claim on-board verification.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'C:/Users/Admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'src/web/local_web_server.cpp'),'utf8');
const html=source.match(/R"HTML\(([\s\S]*?)\)HTML"/)[1];
const config={device_id:'ESP32-TEST00000001',device_name:'Bench master',role:'MASTER',firmware_version:'1.0.0-part1',configuration_revision:2};
for(const k of ['server_led_enabled','server_led_active_high','tm1637_enabled','enabled','registration_open','wifi_enabled','wifi_dhcp','ethernet_enabled','ethernet_dhcp','dht_enabled','lora_enabled','lora_fixed_mode','lora_lbt','lora_rssi','mqtt_enabled','mqtt_tls','server_https'])config[k]=!['mqtt_enabled','mqtt_tls','server_https','ethernet_enabled','dht_enabled','lora_enabled'].includes(k);
Object.assign(config,{wifi_dns2:'',sleep_enabled:false,sleep_start_minute:0,sleep_duration_seconds:3600,sleep_days_mask:127,utc_offset_minutes:330,node_address:'1',master_id:config.device_id,heartbeat_seconds:30,telemetry_seconds:10,command_timeout_ms:5000,retry_count:3,message_max_age_ms:60000,route_timeout_seconds:60,server_port:5000,mqtt_port:1883,mqtt_keepalive:30,dht_pin:4,lora_uart:2,lora_baud:9600,lora_rx_pin:16,lora_tx_pin:17,lora_aux_pin:27,lora_m0_pin:25,lora_m1_pin:26,lora_address_high:0,lora_address_low:0,lora_channel:0,lora_air_rate:0,lora_tx_power:22,max_hops:5,internet_probe_host:'',internet_probe_port:443,internet_probe_tls:true});
(async()=>{
 const browser=await chromium.launch({channel:'msedge',headless:true});
 try{
  for(const width of [1280,375]){
   const ctx=await browser.newContext({viewport:{width,height:900}}),page=await ctx.newPage();let invalid=false,unchanged=false,lastSave=null,lastReset=null;
   let sensorSnapshot={valid:true,status:'OK',parameters:{temperature:29.4,humidity:54}};
   let connection={wifi_connected:true,server_connected:true,mqtt_connected:true,unix_time:1793205000,clock_valid:true,radio:{enabled:false,state:'DISABLED',error:'DISABLED',hardware_link_verified:false,public_settings_read:false}};
   const errors=[];page.on('pageerror',e=>errors.push(e.message));
   await page.route('http://esp32.test/**',async route=>{
    const req=route.request(),url=new URL(req.url());let status=200,body={};
    if(['/','/network','/device','/radio','/nodes','/system'].includes(url.pathname))return route.fulfill({contentType:'text/html',body:html});
    if(url.pathname==='/api/auth/session')body={csrf:'fixture-csrf',authenticated:true};
    else if(url.pathname==='/api/config'&&req.method()==='GET')body=config;
    else if(url.pathname==='/api/connectivity')body=connection;
    else if(url.pathname==='/api/radio/inspect')body={requested:true,rf_transmission:false,register_writes:false};
    else if(url.pathname==='/api/status')body={device:{id:config.device_id,role:'MASTER'},sensor:sensorSnapshot,ethernet_status:'NOT_AVAILABLE',network:{command_state:'COMPLETED'}};
    else if(url.pathname==='/api/config'){lastSave=req.postDataJSON();assert.equal(req.headers()['x-config-request'],'1');if(invalid){status=400;body={error:'Intervals out of range'};}else body={saved:true,restart_required:!unchanged};}
    else if(url.pathname==='/api/commands')body={command_id:'MSG-TEST',state:'QUEUED'};
    else if(url.pathname==='/api/system/factory-reset'){lastReset=req.postDataJSON();body={accepted:true};}
    else if(url.pathname==='/api/network/scan')body=[];
    else body={accepted:true};
    await route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
   });
   await page.goto('http://esp32.test/');
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));
   assert.equal(await page.locator('.esp-topbar').count(),0,'global header removed');
   const boxes=await page.evaluate(()=>{const n=document.querySelector('.esp-sidebar').getBoundingClientRect(),h=document.querySelector('h1').getBoundingClientRect();return {navBottom:n.bottom,navRight:n.right,headingTop:h.top,headingLeft:h.left,overflow:document.documentElement.scrollWidth>innerWidth};});assert.ok(width>700?boxes.headingLeft>=boxes.navRight:boxes.headingTop>=boxes.navBottom,'sidebar must not overlap page');assert.equal(boxes.overflow,false);
   assert.equal(await page.locator('.esp-menu a[aria-current=page]').innerText(),'Dashboard');
   assert.equal(await page.locator('#sensorSummary').count(),0);assert.ok(!(await page.locator('#overview').innerText()).includes('temperature'),'sensor readings excluded from detail view');
   await page.waitForFunction(()=>document.getElementById('link-server').dataset.state==='connected');assert.equal(await page.locator('#link-lora').getAttribute('data-state'),'off');
   assert.equal(await page.locator('#field_admin_password').inputValue(),'');
   assert.equal(await page.locator('#field_gateway_ca').evaluate(e=>e.tagName),'TEXTAREA');
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
   if(process.env.CAPTURE_SCREENSHOTS==='1')await page.screenshot({path:path.join(root,'docs',`local-dashboard-mock-${width}.png`)});
   connection={...connection,server_connected:false,mqtt_connected:false,radio:{enabled:true,state:'READY_LINK_UNVERIFIED',error:'NONE',hardware_link_verified:false,public_settings_read:true}};
   await page.locator('#checkConnection').click();await page.waitForFunction(()=>document.getElementById('link-lora').dataset.state==='pending');assert.equal(await page.locator('#link-server').getAttribute('data-state'),'pending');
   await page.locator('#inspectRadio').click();await page.waitForFunction(()=>document.getElementById('connectionResult').textContent.includes('No RF'));assert.equal(lastSave,null);
   await page.goto('http://esp32.test/device');await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));
   await page.locator('#field_telemetry_seconds').fill('15');
   page.once('dialog',d=>d.accept());invalid=true;await page.getByRole('button',{name:'Save and apply',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent==='Intervals out of range');
   assert.equal(lastSave.telemetry_seconds,15);for(const k of ['server_led_enabled','server_led_active_high','tm1637_enabled']){assert.equal(typeof lastSave[k],'boolean');assert.equal(await page.locator('#field_'+k).getAttribute('type'),'checkbox');}assert.equal(typeof lastSave.enabled,'boolean');assert.ok(!('admin_password' in lastSave));
   page.once('dialog',d=>d.accept());invalid=false;unchanged=true;await page.getByRole('button',{name:'Save and apply',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.includes('no restart required'));
   await page.locator('#destination').fill('ESP32-NODE');await page.getByRole('button',{name:'Send command',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('result').textContent.includes('QUEUED'));
   await page.goto('http://esp32.test/system');await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));
   await page.getByRole('button',{name:'Factory reset',exact:true}).click();assert.equal(lastReset,null);
   await page.locator('#resetId').fill(config.device_id);page.once('dialog',d=>d.accept());await page.getByRole('button',{name:'Factory reset',exact:true}).click();
   await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Reset accepted'));
   assert.deepEqual(lastReset,{confirm_device_id:config.device_id});assert.deepEqual(errors,[]);
   if(process.env.CAPTURE_SCREENSHOTS==='1')await page.screenshot({path:path.join(root,'docs',`local-page-mock-${width}.png`),fullPage:false});
   console.log(`PASS local page ${width}px: fields, types, secrets, validation, save, commands, reset, overflow`);
   for(const route of ['network','device','radio','nodes','system']){await page.goto('http://esp32.test/'+route);await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));assert.equal(await page.locator('.esp-menu a[aria-current=page]').getAttribute('href'),'/'+route);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'no overflow on '+route);}
   assert.deepEqual(errors,[]);console.log(`PASS sidebar navigation ${width}px: Network, Device, Radio, Nodes and System`);
   await ctx.close();
  }
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
