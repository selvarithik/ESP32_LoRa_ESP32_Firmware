// UI behavior with mocked HTTP. Does not exercise physical NVS or ESP32 handlers.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'C:/Users/Admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const html=fs.readFileSync(path.join(__dirname,'../src/web/access_page.h'),'utf8').match(/R"ACCESS\(([\s\S]*?)\)ACCESS"/)[1];
(async()=>{const browser=await chromium.launch({channel:'msedge',headless:true});try{for(const width of [360,1280]){
 const context=await browser.newContext({viewport:{width,height:900}}),page=await context.newPage();
 let authenticated=false,mustChange=true,password='admin',logoutCount=0,pendingCredentials=null,pollCount=0;
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await context.route('http://esp32.test/**',async route=>{const request=route.request(),url=new URL(request.url());
  if(['/login','/account'].includes(url.pathname))return route.fulfill({contentType:'text/html',body:html});
  if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:'<h1>Authenticated dashboard</h1>'});
  if(url.pathname==='/api/connectivity')return route.fulfill({contentType:'application/json',body:JSON.stringify({wifi_connected:true,server_connected:false,mqtt_connected:false,clock_valid:false,radio:{enabled:false,state:'DISABLED',error:'DISABLED'}})});
  let status=200,data={};
  if(url.pathname==='/api/auth/session')data={authenticated,csrf:'test-csrf',username:'admin',password_change_required:mustChange};
  else {assert.equal(request.headers()['x-csrf-token'],'test-csrf');const body=request.method()==='POST'?request.postDataJSON():pendingCredentials;
   if(url.pathname==='/api/auth/login/start'){pendingCredentials=body;pollCount=0;status=202;data={pending:true};}
   else if(url.pathname==='/api/auth/login/result'&&++pollCount<2){status=202;data={pending:true};}
   else if(url.pathname==='/api/auth/login/result'){if(body.username!=='admin'||body.password!==password){status=401;data={error:'Invalid user ID or password.'};}else{authenticated=true;data={success:true,password_change_required:mustChange};}}
   else if(url.pathname==='/api/account'){assert.equal(body.password,password);assert.equal(body.new_password,'fixture-new-password');password=body.new_password;authenticated=false;mustChange=false;data={success:true};}
   else if(url.pathname==='/api/auth/logout'){authenticated=false;logoutCount++;data={success:true};}
   else throw Error('Unexpected endpoint '+url.pathname);
  }await route.fulfill({status,contentType:'application/json',body:JSON.stringify(data)});
 });
 await page.goto('http://esp32.test/login');
 if(process.env.CAPTURE_SCREENSHOTS==='1')await page.screenshot({path:path.join(__dirname,'../docs',`access-page-mock-${width}.png`)});
 assert.equal(await page.locator('.esp-topbar').count(),0,'login has no global header');
 const loginBounds=await page.evaluate(()=>({form:document.querySelector('main').getBoundingClientRect().top,overflow:document.documentElement.scrollWidth>innerWidth}));assert.ok(loginBounds.form>=0,'login remains reachable');assert.equal(loginBounds.overflow,false);
 await page.locator('#username').fill('admin');await page.locator('#password').fill('wrong');
 await page.locator('#visibility').click();assert.equal(await page.locator('#password').getAttribute('type'),'text');
 await page.locator('#submit').click();await page.waitForFunction(()=>document.getElementById('feedback').textContent.includes('Incorrect'));
 await page.locator('#password').fill('admin');await page.locator('#submit').click();await page.waitForURL('**/account');
 await page.waitForFunction(()=>document.getElementById('identity').textContent.includes('temporary'));
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
 assert.match(await page.locator('main').textContent(),/HTTP does not encrypt/);
 await page.locator('#password').fill('admin');await page.locator('#new_password').fill('fixture-new-password');await page.locator('#confirmation').fill('fixture-new-password');
 await page.locator('#submit').click();await page.waitForURL('**/login');
 await page.locator('#username').fill('admin');await page.locator('#password').fill('fixture-new-password');await page.locator('#submit').click();await page.waitForURL('http://esp32.test/');
 await page.goto('http://esp32.test/account');await page.locator('#logout').click();await page.waitForURL('**/login');assert.equal(logoutCount,1);assert.deepEqual(errors,[]);
 console.log(`PASS access mock UI ${width}px: visibility, errors, first-use change, new login, logout, HTTP warning, responsive layout`);await context.close();
 }}finally{await browser.close();}})().catch(error=>{console.error(error);process.exitCode=1;});
