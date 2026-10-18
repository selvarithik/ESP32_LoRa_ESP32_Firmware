// Browser tests with mocked responses. These do not test NVS, TLS or on-board handlers.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'C:/Users/Admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'..');
const html=fs.readFileSync(path.join(root,'src/web/mqtt_page.h'),'utf8').match(/R"MQTT\(([\s\S]*?)\)MQTT"/)[1];
const defaults={profile_name:'Primary Broker',provider:'EMQX',host:'broker.test',port:8883,mqtt_version:'3.1.1',tls:true,ca_cert:'TEST CERTIFICATE',username:'',client_id:'master-test',keepalive_s:60,timeout_s:10,qos_default:1,topic_prefix:'selvarithik/gateway',gateway_id:'GW001'};
(async()=>{const browser=await chromium.launch({channel:'msedge',headless:true});try{
for(const width of [360,1280]){
 const context=await browser.newContext({viewport:{width,height:900}}),page=await context.newPage();
 let state={revision:0,defaults,transaction:{state:'IDLE'},comparison:{status:'UNKNOWN',message:'No verified Gateway snapshot received'},active_source:'legacy configuration; migration pending'},reject=true,saveCount=0;
 page.setDefaultTimeout(10000);const errors=[],external=[];page.on('pageerror',e=>errors.push(e.message));
 await page.route('**/*',async route=>{
  const req=route.request(),url=new URL(req.url());if(url.origin!=='http://esp32.test'){external.push(req.url());return route.abort();}
  if(url.pathname==='/mqtt')return route.fulfill({contentType:'text/html',body:html});
  if(url.pathname==='/api/auth/session')return route.fulfill({contentType:'application/json',body:JSON.stringify({csrf:'fixture-csrf',authenticated:true})});
  let status=200,body=state;
  if(req.method()==='POST'){
   assert.equal(req.headers()['x-config-request'],'1');assert.equal(req.headers()['x-csrf-token'],'fixture-csrf');const data=req.postDataJSON();assert.equal(data.expected_revision,state.revision);
   if(url.pathname.endsWith('/candidate')){
    saveCount++;assert.ok(!('password' in data.profile));assert.equal(data.profile.tls,true);assert.equal(data.profile.qos_default,1);
    if(reject){status=400;body={error:{code:'INVALID_PROFILE',message:'Invalid certificate',field:'ca_cert'}};}
    else{state={...state,revision:1,pending:data.profile,transaction:{state:'PENDING',transaction_id:'test-id'}};body=state;}
   }else if(url.pathname.endsWith('/cancel')){assert.equal(data.transaction_id,'test-id');state={...state,revision:2,pending:null,transaction:{state:'ROLLED_BACK'}};body=state;}
   else throw Error('Unexpected mutation '+url.pathname);
  }
  await route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
 });
 await page.goto('http://esp32.test/mqtt');await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Ready.'));
 assert.equal(await page.locator('#field_password').inputValue(),'');assert.equal(await page.locator('#field_password').isDisabled(),true);
 assert.equal(await page.locator('#field_tls').isDisabled(),true);assert.equal(await page.locator('#confirm').isDisabled(),true);
 assert.equal(await page.locator('#test').isDisabled(),true);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
 await page.locator('#field_host').fill('unsaved.example');await page.locator('#refreshStatus').click();
 await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Status refreshed.'));
 assert.equal(await page.locator('#field_host').inputValue(),'unsaved.example');assert.equal(saveCount,0);assert.equal(state.revision,0);
 await page.getByRole('button',{name:'Save candidate',exact:true}).click();
 await page.waitForFunction(()=>document.getElementById('feedback').textContent.includes('Invalid certificate'));
 assert.equal(await page.locator('#field_ca_cert').getAttribute('aria-invalid'),'true');assert.equal(await page.locator('#feedback').evaluate(e=>e===document.activeElement),true);
 reject=false;await page.getByRole('button',{name:'Save candidate',exact:true}).click();await page.waitForFunction(()=>document.getElementById('feedback').textContent.startsWith('Candidate saved.'));
 assert.equal(saveCount,2);assert.equal(await page.locator('#save').isDisabled(),true);assert.ok(!(await page.locator('#state').textContent()).includes('password'));
 await page.getByRole('button',{name:'Cancel candidate',exact:true}).click();await page.waitForFunction(()=>document.getElementById('transaction').textContent==='ROLLED_BACK');
 state.comparison={status:'MISMATCH',message:'Master config differs'};await page.reload();await page.waitForFunction(()=>document.getElementById('comparison').textContent==='Master config differs');
 assert.deepEqual(errors,[]);assert.deepEqual(external,[]);
 if(process.env.CAPTURE_SCREENSHOTS==='1')await page.screenshot({path:path.join(root,'docs',`mqtt-page-mock-${width}.png`),fullPage:true});
 console.log(`PASS MQTT mock UI ${width}px: no secrets, mandatory TLS, staging, cancellation, field errors, mismatch rendering, offline assets`);await context.close();
}
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
