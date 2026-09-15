// Real C++ WASM, not mock chart data. Preserve evidence for both preview and Pages.
const {chromium}=require('@playwright/test');
const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/';
 const out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const checks=[],errors=[],requests=[];let stage='launch';
 const save=()=>fs.writeFileSync(out+'/powertrain-acceptance.json',JSON.stringify({url:base,stage,checks,errors,requests,accepted:stage==='complete'&&checks.every(c=>c.pass)&&!errors.length&&!requests.length},null,2));
 const ok=(value,name)=>{checks.push({name,pass:!!value});save();if(!value)throw Error(name);console.log('PASS:',name);};
 const watchdog=setTimeout(()=>{checks.push({name:'timeout at '+stage,pass:false});save();process.exit(1);},240000);
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1440,height:1000}});
 const capture=async name=>{await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';window.scrollTo({top:0,behavior:'instant'});});await page.waitForFunction(()=>window.scrollY===0);await page.screenshot({path:out+'/'+name,fullPage:true});};
 page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))requests.push(r.status()+' '+r.url());});
 try{
  stage='automatic entry';await page.goto(base,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.labState?.ready&&labState.running&&labState.snapshot?.[0]>.5,null,{timeout:60000});
  ok(await page.locator('#run').textContent()==='Pause simulation','visible joint autostarts without a click');
  await page.click('#reset');await page.waitForFunction(()=>labState.snapshot?.[0]===0&&!labState.running);
  ok(true,'explicit reset remains paused');
  stage='media';const response=await page.request.get(new URL('media/evidence.json',base).href);const media=await response.json();
  ok(media.tour.layout==='sequential, not a simultaneous grid'&&media.tour.bytes<4*1024*1024,'sequential README GIF has bounded size');
  for(const id of ['tracking','disturbance','contact','impact','fault']){
   const m=media.videos[id];ok(m.window_sim_s[0]<=m.event_sim_s&&m.event_sim_s<m.window_sim_s[1]&&m.probe.duration_s<m.source_probe.duration_s&&m.probe.duration_s<4,id+' excerpt includes its native event');
   ok(await page.locator('#film-'+id+' video').evaluate(v=>v.loop&&v.muted),id+' video is configured for muted looping');
  }
  const gif=await page.request.get(new URL('media/feature-tour.gif',base).href);ok(gif.ok()&&(await gif.body()).subarray(0,3).toString()==='GIF','README GIF publicly served');
  await page.locator('#recordings').evaluate(e=>e.open=true);
  const video=page.locator('#film-fault video');await video.scrollIntoViewIfNeeded();
  await video.evaluate(v=>{v.addEventListener('timeupdate',()=>{if(v.dataset.armed==='1'&&v.currentTime<.5)v.dataset.wrapped='1';});v.muted=true;v.play();});
  await page.waitForFunction(()=>document.querySelector('#film-fault video').readyState>=2);
  await video.evaluate(v=>{v.currentTime=v.duration-.18;v.dataset.armed='1';});
  await page.waitForFunction(()=>document.querySelector('#film-fault video').dataset.wrapped==='1',null,{timeout:15000});ok(true,'event video actually loops through the restart');
  stage='electrical experiment';await page.goto(new URL('electronics.html',base).href,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.powertrainState?.completed>=1&&!powertrainState.busy,null,{timeout:60000});
  ok(await page.evaluate(()=>powertrainState.tables[0].length===2400&&powertrainState.tables[1].length>1900),'native control and switching samples populated');
  ok(await page.evaluate(()=>Math.abs(powertrainState.tables[0].at(-1)[4]-4)<.15),'C++ current loop tracks default target');
  ok((await page.locator('#flow-duty').textContent()).includes('%'),'pipeline carries numerical SVPWM duty values');
  await page.locator('#sample').evaluate(e=>{e.value='40';e.dispatchEvent(new Event('input',{bubbles:true}));});
  const before=await page.locator('#sample-readout').textContent();await page.locator('#sample').evaluate(e=>{e.value='80';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok(before!==await page.locator('#sample-readout').textContent(),'inspection slider follows computed switching samples');
  await page.selectOption('#wave','voltage');await page.check('#zoom');ok(await page.locator('#switch-chart path').count()>=2,'phase/line and dead-time plot is populated');
  await capture('powertrain-electrical.png');
  const first=await page.evaluate(()=>powertrainState.tables[0].at(-1)[4]);
  await page.fill('#iq','6');ok(await page.locator('#exp-status').evaluate(e=>e.classList.contains('stale')),'editing an input marks old results stale');
  const count=await page.evaluate(()=>powertrainState.completed);await page.click('#calculate');
  await page.waitForFunction(n=>powertrainState.completed>n&&!powertrainState.busy,count,{timeout:60000});
  ok(await page.evaluate(x=>powertrainState.tables[0].at(-1)[4]>x+1.5,first),'new current input recomputes the plant, not just a label');
  stage='thermal';await page.click('[data-kind="1"]');await page.click('#calculate');
  await page.waitForFunction(()=>powertrainState.tables?.[2]?.length===601&&!powertrainState.busy,null,{timeout:60000});
  ok(await page.evaluate(()=>powertrainState.tables[2].at(-1)[1]>powertrainState.tables[2][0][1]),'native prescribed-current thermal simulation heats up');
  await capture('powertrain-thermal.png');
  stage='motor map';await page.click('[data-kind="2"]');await page.click('#calculate');
  await page.waitForFunction(()=>powertrainState.tables?.[3]?.length===28&&!powertrainState.busy,null,{timeout:90000});
  ok(await page.evaluate(()=>powertrainState.tables[3].some(r=>r[6])&&powertrainState.tables[3].filter(r=>r[0]===0).every(r=>r[5]===-1)),'torque/RPM map keeps qualified points and missing stall efficiencies');
  ok(await page.locator('#torque-chart path').count()===4&&await page.locator('#efficiency-chart path').count()===4,'four current levels have torque and efficiency traces');
  const domains=await Promise.all(['torque-chart','efficiency-chart'].map(id=>page.locator('#'+id+' text').allTextContents()));
  ok(domains.every(values=>values.includes('0')&&values.includes('3600')),'torque and efficiency retain identical RPM domains including unqualified regions');
  const downloadPromise=page.waitForEvent('download');await page.click('#export-exp');const download=await downloadPromise;await download.saveAs(out+'/motor-map.csv');
  const csv=fs.readFileSync(out+'/motor-map.csv','utf8');ok(csv.includes('numerical_residual_W')&&csv.trim().split('\n').length===29,'CSV includes 28 computed points and power residuals');
  await capture('powertrain-map.png');
  for(const width of [390,768,1440]){await page.setViewportSize({width,height:900});await page.waitForTimeout(250);ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'powertrain layout at '+width+' px');}
  if(process.env.EXPECTED_SHA){const d=await(await page.request.get(new URL('build.json',base).href)).json();ok(d.source_commit===process.env.EXPECTED_SHA,'public build matches merged commit');}
  ok(!errors.length&&!requests.length,'no script exceptions or missing assets');stage='complete';
 }catch(e){checks.push({name:e.message,pass:false});await page.screenshot({path:out+'/powertrain-failure.png',fullPage:true}).catch(()=>{});process.exitCode=1;}
 save();console.log(JSON.stringify({stage,checks,errors,requests},null,2));clearTimeout(watchdog);await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
