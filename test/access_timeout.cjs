const fs=require('node:fs'),assert=require('node:assert/strict');
const {chromium}=require('C:/Users/Admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const html=fs.readFileSync(require('node:path').join(__dirname,'../src/web/access_page.h'),'utf8').match(/R"ACCESS\(([\s\S]*?)\)ACCESS"/)[1];
(async()=>{const browser=await chromium.launch({channel:'msedge',headless:true});try{
 const context=await browser.newContext(),page=await context.newPage();await page.clock.install();
 await context.route('http://esp32.test/**',async route=>{const path=new URL(route.request().url()).pathname;
  if(path==='/login')return route.fulfill({contentType:'text/html',body:html});
  if(path==='/api/auth/session')return route.fulfill({contentType:'application/json',body:JSON.stringify({csrf:'test-csrf',authenticated:false})});
  if(path==='/api/auth/login/start')return route.fulfill({status:202,contentType:'application/json',body:'{"pending":true}'});
  if(path==='/api/auth/login/result')return route.fulfill({status:202,contentType:'application/json',body:'{"pending":true}'});
  throw Error('Unexpected endpoint');
 });
 await page.goto('http://esp32.test/login');await page.locator('#submit').waitFor({state:'visible'});await page.waitForFunction(()=>!document.getElementById('submit').disabled);
 await page.locator('#username').fill('admin');await page.locator('#password').fill('fixture-password');await page.locator('#submit').click();
 for(let n=0;n<65;n++){await page.clock.fastForward(500);await page.waitForTimeout(10);}
 await page.waitForFunction(()=>document.getElementById('feedback').textContent.includes('timed out'));assert.equal(await page.locator('#submit').isEnabled(),true);
 console.log('PASS mocked login: pending verification expires, releases button, never loops forever');await context.close();
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
