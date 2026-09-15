// End-to-end first-screen pipeline. Always use the real built C++/WASM Worker.
const {chromium}=require('@playwright/test');
const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/';
 const out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const checks=[],errors=[],requests=[];let stage='start';
 const save=()=>fs.writeFileSync(out+'/home-acceptance.json',JSON.stringify({url:base,stage,checks,errors,requests,accepted:stage==='complete'&&checks.every(c=>c.pass)&&!errors.length&&!requests.length},null,2));
 const ok=(v,name)=>{checks.push({name,pass:!!v});save();if(!v)throw Error(name);console.log('PASS:',name);};
 const watchdog=setTimeout(()=>{checks.push({name:'Timeout at '+stage,pass:false});save();process.exit(1);},180000);
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1440,height:1000}});
 page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))requests.push(r.status()+' '+r.url());});
 try{
  stage='entry';await page.goto(base,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.labState?.modelLoaded&&labState.running&&labState.signal?.length===28&&labState.snapshot?.[0]>.8,null,{timeout:60000});
  ok(await page.evaluate(()=>Math.abs(labState.snapshot[1]-.4)<.025),'entry follows .4 rad without hidden feed-forward bias');
  ok(await page.evaluate(()=>labState.snapshot[0]===labState.signal[0]),'scene and controller sidecar share a snapshot time');
  ok(await page.locator('[data-live-stage]').count()===5,'one five-stage live pipeline');
  ok(await page.evaluate(()=>document.getElementById('live-pipeline').getBoundingClientRect().top<document.getElementById('viewport').getBoundingClientRect().top),'pipeline precedes 3D viewport');
  ok(await page.locator('#recordings').evaluate(e=>!e.open),'recordings are secondary and collapsed on entry');
  ok(await page.locator('[data-lesson]').count()===0&&await page.locator('iframe').count()===0,'no duplicate scenario launchers or hidden second simulator');
  await page.click('#run');await page.waitForTimeout(150);
  const frozen=await page.evaluate(()=>JSON.stringify([labState.snapshot,labState.signal]));
  for(const key of ['command','foc','bridge','motion','sensor']){
   await page.click('[data-live-stage="'+key+'"]');
   ok(await page.locator('[data-live-stage="'+key+'"]').getAttribute('aria-pressed')==='true','select '+key+' stage in place');
  }
  ok(await page.evaluate(v=>JSON.stringify([labState.snapshot,labState.signal])===v,frozen),'inspection does not advance or change the frozen experiment');
  await page.locator('[data-live-stage="sensor"]').focus();await page.keyboard.press('ArrowRight');
  ok(await page.locator('[data-live-stage="command"]').getAttribute('aria-pressed')==='true','ordered stage navigation works with keyboard');
  await page.click('[data-live-stage="foc"]');
  ok(await page.evaluate(()=>document.getElementById('pipeline-values').textContent.includes(labState.signal[4].toFixed(2)+' / '+labState.signal[5].toFixed(2)+' V')),'displayed dq voltage comes from the same C++ controller');
  stage='pending';await page.locator('#target').evaluate(e=>{e.value='.7';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok(await page.locator('#pipeline-pending').isVisible(),'paused edits are marked pending instead of rewriting history');
  const time=await page.evaluate(()=>labState.snapshot[0]);await page.click('#step');
  await page.waitForFunction(t=>labState.snapshot[0]>t,time);
  ok(await page.evaluate(()=>Math.abs(labState.signal[19]-.7)<1e-5)&&await page.locator('#pipeline-pending').isHidden(),'one step applies the pending command');
  stage='mode';await page.selectOption('#mode','0');
  ok(await page.locator('#current').isVisible()&&await page.locator('#target').isDisabled()&&await page.locator('#impedance-gains').isHidden(),'current mode exposes its own command, not an angle controller');
  await page.click('#step');await page.waitForFunction(()=>labState.signal[18]===0);
  await page.click('[data-live-stage="command"]');
  ok((await page.locator('#pipe-mode').textContent()).includes('Current'),'pipeline command identifies the active C++ mode');
  ok((await page.locator('#pipeline-values').textContent()).includes('outer torque loop bypassed'),'current command does not invent a torque-conversion stage');
  await page.selectOption('#mode','2');
  ok(await page.locator('#velocity').isVisible()&&await page.locator('#current').isDisabled(),'speed command is not buried under other modes');
  stage='fault';await page.click('[data-case="tracking"]');await page.waitForFunction(()=>labState.snapshot?.[0]>.8&&labState.signal?.[18]===4);
  await page.click('#fault');await page.waitForFunction(()=>labState.snapshot[12]===9);
  await page.click('[data-live-stage="bridge"]');
  ok((await page.locator('#pipeline-values').textContent()).includes('gates OFF'),'pipeline exposes actual gate inhibition after fault');
  await page.click('#reset');await page.waitForFunction(()=>labState.snapshot?.[0]===0&&!labState.running);
  ok(await page.evaluate(()=>labState.signal[0]===0&&labState.signal[1]===-1),'reset clears old controller sample, not just the viewer');
  await page.click('#run');await page.waitForFunction(()=>labState.snapshot?.[0]>.8);await page.click('#run');await page.waitForTimeout(150);
  await page.click('[data-live-stage="foc"]');
  for(const width of [1440,768,390]){
   await page.setViewportSize({width,height:1000});await page.waitForTimeout(250);
   await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';window.scrollTo({top:0,behavior:'instant'});});
   ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'homepage has no horizontal overflow at '+width);
   await page.screenshot({path:out+'/home-pipeline-'+width+'.png',fullPage:width===1440});
  }
  if(process.env.EXPECTED_SHA){const m=await(await page.request.get(new URL('build.json',base).href)).json();ok(m.source_commit===process.env.EXPECTED_SHA,'public homepage is bound to merged source');}
  ok(!errors.length&&!requests.length,'no page exceptions or missing assets');stage='complete';
 }catch(e){checks.push({name:e.message,pass:false});await page.screenshot({path:out+'/home-failure.png',fullPage:true}).catch(()=>{});process.exitCode=1;}
 save();clearTimeout(watchdog);await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
