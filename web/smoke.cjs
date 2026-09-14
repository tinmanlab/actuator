// Real browser acceptance; physical intervals are relative to each command.
const {chromium}=require('@playwright/test');
const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/';
 const out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const errors=[],failures=[],checks=[],media={};let stage='launch';
 const save=()=>fs.writeFileSync(out+'/browser-acceptance.json',JSON.stringify({url:base,stage,checks,errors,failures,media,accepted:stage==='complete'&&checks.every(x=>x.pass)},null,2));
 const mark=s=>{stage=s;console.log('BROWSER STAGE:',s);save();};
 const ok=(v,name)=>{checks.push({name,pass:!!v});console.log(v?'PASS:':'FAIL:',name);save();if(!v)throw Error(name);};
 const watchdog=setTimeout(()=>{checks.push({name:'Browser acceptance exceeded 180 seconds at '+stage,pass:false});save();process.exit(1);},180000);
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1440,height:1100}});page.setDefaultTimeout(20000);
 page.on('pageerror',e=>errors.push(e.message));
 page.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))failures.push(r.status()+' '+r.url());});
 try{
  mark('load C++ and viewer');await page.goto(base,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.labState?.ready&&window.labState?.modelLoaded,null,{timeout:60000});
  ok(await page.locator('#run').isEnabled(),'C++ WASM and real 3D model ready');
  mark('initial tracking');await page.click('#run');await page.waitForFunction(()=>labState.snapshot?.[0]>.8);
  ok(await page.evaluate(()=>Math.abs(labState.snapshot[1]-.4)<.03),'live .4 rad tracking');
  await page.click('#run');await page.waitForTimeout(150);const t=await page.evaluate(()=>labState.snapshot[0]);await page.waitForTimeout(150);
  ok(await page.evaluate(x=>labState.snapshot[0]===x,t),'pause freezes simulation time');
  await page.click('#step');await page.waitForTimeout(150);
  ok(await page.evaluate(x=>Math.abs(labState.snapshot[0]-x-.001)<1e-9,t),'single step advances one millisecond');
  mark('changed target');const targetAt=await page.evaluate(()=>labState.snapshot[0]);
  await page.locator('#target').evaluate(e=>{e.value='.7';e.dispatchEvent(new Event('input',{bubbles:true}));});await page.click('#run');
  await page.waitForFunction(start=>labState.snapshot?.[0]>start+.8,targetAt);
  ok(await page.evaluate(()=>Math.abs(labState.snapshot[1]-.7)<.04),'target slider changes physical trajectory');
  mark('pulse load');const pulseAt=await page.evaluate(()=>labState.snapshot[0]);await page.click('#push');
  await page.waitForFunction(start=>labState.rows.some(r=>r[0]>=start&&r[19]>3.9),pulseAt);ok(true,'push reaches C++ load input');
  mark('contact');await page.click('[data-case="contact"]');
  await page.waitForFunction(()=>Math.abs((labState.snapshot?.[18]??99)-.85)<1e-6&&labState.snapshot?.[0]>1.2);
  ok(await page.evaluate(()=>labState.snapshot[1]>.59&&labState.snapshot[1]<.64&&labState.snapshot[20]>1),'contact constrains angle and produces reaction');
  mark('motor inspection');await page.click('#view-motor');await page.click('#cutaway');
  ok(await page.locator('#cutaway').getAttribute('aria-pressed')==='true','cutaway inspection works');
  await page.click('#view-bench');await page.click('#cutaway');await page.click('#run');await page.waitForTimeout(100);
  mark('screenshot and CSV');await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';window.scrollTo({top:0,behavior:'instant'});});await page.waitForFunction(()=>window.scrollY===0);await page.waitForTimeout(400);
  await page.screenshot({path:out+'/browser-lab.png',fullPage:false,timeout:15000});
  const downloadPromise=page.waitForEvent('download');await page.click('#export');const download=await downloadPromise;await download.saveAs(out+'/browser-trace.csv');
  const csv=fs.readFileSync(out+'/browser-trace.csv','utf8');ok(csv.includes('iq_reference_A'),'CSV export has explicit current units');
  const times=csv.trim().split('\n').slice(1).map(line=>Number(line.split(',')[0]));
  ok(times.length>100&&times.every((t,i)=>!i||t>times[i-1]),'CSV timestamps strictly increase after commands and reset');
  mark('fault and reset');await page.click('#run');await page.click('#fault');await page.waitForFunction(()=>labState.snapshot?.[12]===9);
  ok(await page.evaluate(()=>labState.snapshot[13]===0),'fault turns gates off');
  await page.click('#reset');await page.waitForFunction(()=>labState.snapshot?.[0]===0&&!labState.running);ok(true,'reset creates paused fresh experiment');
  mark('predictive controller');await page.locator('aside details summary').click();await page.selectOption('#algorithm','1');await page.click('[data-case="tracking"]');
  await page.waitForFunction(()=>Math.abs((labState.snapshot?.[18]??99)-.4)<1e-6&&labState.snapshot?.[0]>.8);
  ok(await page.evaluate(()=>labState.snapshot[12]===0&&Math.abs(labState.snapshot[1]-.4)<.04),'predictive algorithm runs');await page.click('#run');
  mark('reverse contact and return');
  await page.selectOption('#algorithm','0');await page.click('[data-case="reverse"]');
  await page.waitForFunction(()=>labState.scenario==='reverse'&&labState.snapshot?.[0]>2.5);
  ok(await page.evaluate(()=>labState.snapshot[29]===1&&labState.snapshot[1]>-5.21&&labState.snapshot[1]<-5.17&&labState.snapshot[20]<-1&&labState.snapshot[12]===0),'reverse motion hits the opposite stop face without traversing it');
  ok(await page.evaluate(()=>labState.rows.every(r=>r[30]<.03)),'recorded reverse contact deflection remains below 0.03 rad');
  await page.click('#colliders');ok(await page.locator('#colliders').getAttribute('aria-pressed')==='true','collision envelopes can be inspected');
  await page.click('#run');await page.locator('#viewport').screenshot({path:out+'/reverse-contact.png'});
  await page.click('#colliders');const reverseAt=await page.evaluate(()=>labState.snapshot[0]);
  await page.locator('#velocity').fill('3');await page.locator('#velocity').press('Tab');await page.click('#run');
  await page.waitForFunction(t=>labState.snapshot?.[0]>t+3,reverseAt);
  ok(await page.evaluate(()=>labState.snapshot[1]>.59&&labState.snapshot[1]<.63&&labState.snapshot[20]>1&&labState.snapshot[12]===0),'reverse contact releases and the return meets the front face');
  mark('safe stop insertion');await page.click('[data-case="tracking"]');
  await page.locator('#target').evaluate(e=>{e.value='.8';e.dispatchEvent(new Event('input',{bubbles:true}));});
  await page.waitForFunction(()=>labState.snapshot?.[0]>.8&&labState.snapshot[1]>.75&&labState.snapshot[29]===0);
  ok(await page.evaluate(()=>!document.getElementById('contact').checked&&document.getElementById('notice').hidden),'occupied-stop test starts with the obstacle disabled and no stale notice');
  // The application must reject this click. check() incorrectly requires the
  // final checkbox to remain checked, racing the Worker's rejection reply.
  await page.click('#contact');
  await page.waitForFunction(()=>{const n=document.getElementById('notice');return !n.hidden&&n.textContent.startsWith('Stop not enabled:');});
  const rejectedAt=await page.evaluate(()=>labState.snapshot[0]);
  await page.waitForFunction(t=>labState.snapshot[0]>t+.1,rejectedAt);
  ok(await page.evaluate(()=>labState.snapshot[29]===0&&labState.snapshot[12]===0&&!document.getElementById('contact').checked&&document.getElementById('error').hidden),'occupied-stop insertion is rejected without stopping the engine');
  await page.click('#run');
  for(const id of ['tracking','disturbance','contact','impact','fault']){
   mark('video '+id);const video=page.locator('#film-'+id+' video');await video.scrollIntoViewIfNeeded();
   media[id]=await video.evaluate(v=>({mp4:v.canPlayType('video/mp4; codecs="avc1.64001f"'),vp9:v.canPlayType('video/webm; codecs="vp9"')}));
   console.log('MEDIA SUPPORT:',id,JSON.stringify(media[id]));save();
   // Bounded evidence: decoded frames and an advancing clock, not only play().
   await video.evaluate(v=>{v.muted=true;v.play().catch(e=>{v.dataset.playError=e.message;});});
   try {
    await page.waitForFunction(id=>{const v=document.querySelector('#film-'+id+' video');return v.error||v.dataset.playError||(v.readyState>=2&&v.currentTime>.05&&v.getVideoPlaybackQuality().totalVideoFrames>0);},id,{timeout:15000});
   } finally {
    Object.assign(media[id],await video.evaluate(v=>({source:v.currentSrc,ready:v.readyState,network:v.networkState,paused:v.paused,time:v.currentTime,width:v.videoWidth,height:v.videoHeight,duration:v.duration,frames:v.getVideoPlaybackQuality().totalVideoFrames,error:v.error?.message||v.dataset.playError||null})));save();
   }
   ok(!media[id].error&&media[id].width===1280&&media[id].frames>0&&media[id].time>0&&media[id].duration>0,'actual MuJoCo '+id+' video decodes');await video.evaluate(v=>v.pause());
  }
  mark('layout and asset integrity');await page.setViewportSize({width:390,height:844});await page.waitForTimeout(100);
  ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'narrow layout has no horizontal overflow');
  mark('control guide and pipeline');
  const guide=await browser.newPage({viewport:{width:1440,height:1000}});
  guide.on('pageerror',e=>errors.push(e.message));
  guide.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))failures.push(r.status()+' '+r.url());});
  await guide.goto(new URL('control.html',base).href,{waitUntil:'networkidle'});
  await guide.locator('button[data-stage="pwm"]').click();
  ok(await guide.locator('[data-detail="pwm"]').isVisible(),'pipeline block reveals units and implementation');
  await guide.locator('button[data-stage="sense"]').focus();await guide.keyboard.press('Enter');
  ok(await guide.locator('[data-detail="sense"]').isVisible(),'pipeline selection works from the keyboard');
  await guide.locator('#pipeline').scrollIntoViewIfNeeded();await guide.screenshot({path:out+'/control-guide.png'});
  await guide.locator('#electrical-angle').evaluate(e=>{e.value='90';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok((await guide.locator('#phase-values').textContent()).includes('ia = -4.000 A'),'FOC coordinate example computes the declared phase current');
  await guide.locator('#foc').scrollIntoViewIfNeeded();await guide.screenshot({path:out+'/foc-guide.png'});
  for(const width of [390,768,1440]){await guide.setViewportSize({width,height:900});ok(await guide.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'control guide layout at '+width+' px');}
  await guide.goto(new URL('api.html',base).href,{waitUntil:'networkidle'});
  ok(await guide.locator('[data-set]').count()===11&&await guide.locator('[data-get]').count()===31,'API documents all 11 commands and 31 telemetry fields');
  ok((await guide.locator('#live-example').textContent()).includes('lab_step(100)'),'API page includes the compiled C++ example');
  await guide.screenshot({path:out+'/api-guide.png'});
  for(const width of [390,768,1440]){await guide.setViewportSize({width,height:900});ok(await guide.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'API guide layout at '+width+' px');}
  if(process.env.EXPECTED_SHA){await guide.waitForFunction(()=>document.getElementById('guide-version').textContent.startsWith('Source '));ok((await guide.locator('[data-source]').first().getAttribute('href')).includes(process.env.EXPECTED_SHA),'guide source links bind the deployed commit');}
  await guide.close();
  ok(!errors.length&&!failures.length,'no script errors or missing same-origin assets');
  const manifest=await page.evaluate(async()=>await (await fetch('build.json')).json());
  if(process.env.EXPECTED_SHA){
   ok(manifest.source_commit===process.env.EXPECTED_SHA,'deployed source SHA matches tested commit');
   const hero=await page.request.get(new URL('media/browser-lab.png',base).href);
   ok(hero.status()===200&&(await hero.body()).length>10000,'README screenshot is publicly available');
  }
  mark('complete');
 }catch(e){checks.push({name:e.message,pass:false});save();await page.screenshot({path:out+'/failure.png',fullPage:true,timeout:5000}).catch(()=>{});process.exitCode=1;}
 save();console.log(JSON.stringify({stage,checks,errors,failures,media},null,2));clearTimeout(watchdog);await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
