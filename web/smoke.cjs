// Run against built assets and again against the deployed public Pages URL.
const {chromium}=require('@playwright/test');
const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/';
 const out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1440,height:1100}});
 const errors=[],failures=[],checks=[];
 page.on('pageerror',e=>errors.push(e.message));
 page.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))failures.push(r.status()+' '+r.url());});
 const ok=(v,name)=>{checks.push({name,pass:!!v});if(!v)throw Error(name);};
 try{
  await page.goto(base,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.labState?.ready&&window.labState?.modelLoaded,null,{timeout:60000});
  ok(await page.locator('#run').isEnabled(),'C++ WASM and real 3D model ready');
  await page.click('#run');await page.waitForFunction(()=>labState.snapshot?.[0]>.8);
  ok(await page.evaluate(()=>Math.abs(labState.snapshot[1]-.4)<.03),'live .4 rad tracking');
  await page.click('#run');await page.waitForTimeout(150);const t=await page.evaluate(()=>labState.snapshot[0]);await page.waitForTimeout(150);
  ok(await page.evaluate(x=>labState.snapshot[0]===x,t),'pause freezes simulation time');
  await page.click('#step');await page.waitForTimeout(150);
  ok(await page.evaluate(x=>Math.abs(labState.snapshot[0]-x-.001)<1e-9,t),'single step advances one millisecond');
  await page.locator('#target').evaluate(e=>{e.value='.7';e.dispatchEvent(new Event('input',{bubbles:true}));});await page.click('#run');
  await page.waitForFunction(()=>labState.snapshot?.[0]>1.7);ok(await page.evaluate(()=>Math.abs(labState.snapshot[1]-.7)<.04),'target slider changes physical trajectory');
  await page.click('#push');await page.waitForFunction(()=>labState.snapshot?.[19]>3.9);ok(true,'push reaches C++ load input');
  await page.click('[data-case="contact"]');await page.waitForFunction(()=>labState.snapshot?.[0]>1.2);
  ok(await page.evaluate(()=>labState.snapshot[1]>.59&&labState.snapshot[1]<.64&&labState.snapshot[20]>1),'contact constrains angle and produces reaction');
  await page.click('#view-motor');await page.click('#cutaway');ok(await page.locator('#cutaway').getAttribute('aria-pressed')==='true','cutaway inspection works');
  await page.click('#view-bench');await page.click('#cutaway');
  await page.click('#run');await page.waitForTimeout(100);
  await page.evaluate(()=>window.scrollTo(0,0));await page.waitForTimeout(400);await page.screenshot({path:out+'/browser-lab.png',fullPage:false});
  const downloadPromise=page.waitForEvent('download');await page.click('#export');const download=await downloadPromise;await download.saveAs(out+'/browser-trace.csv');
  const csv=fs.readFileSync(out+'/browser-trace.csv','utf8');
  ok(csv.includes('iq_reference_A'),'CSV export has explicit current units');
  const times=csv.trim().split('\n').slice(1).map(line=>Number(line.split(',')[0]));
  ok(times.length>100&&times.every((t,i)=>!i||t>times[i-1]),'CSV timestamps strictly increase after commands and reset');
  await page.click('#run');await page.click('#fault');await page.waitForFunction(()=>labState.snapshot?.[12]===9);
  ok(await page.evaluate(()=>labState.snapshot[13]===0),'fault turns gates off');
  await page.click('#reset');await page.waitForTimeout(150);ok(await page.evaluate(()=>labState.snapshot[0]===0&&!labState.running),'reset creates paused fresh experiment');
  await page.locator('aside details summary').click();await page.selectOption('#algorithm','1');await page.click('[data-case="tracking"]');await page.waitForFunction(()=>labState.snapshot?.[0]>.8);
  ok(await page.evaluate(()=>labState.snapshot[12]===0&&Math.abs(labState.snapshot[1]-.4)<.04),'predictive algorithm runs');
  await page.click('#run');
  for(const id of ['tracking','disturbance','contact','impact','fault']){
   const video=page.locator('#film-'+id+' video');await video.evaluate(async v=>{await v.play();});
   await page.waitForFunction(id=>{const v=document.querySelector('#film-'+id+' video');return v.readyState>=2&&v.currentTime>0;},id);
   ok(await video.evaluate(v=>v.videoWidth===1280&&v.duration>0),'actual MuJoCo '+id+' video decodes');await video.evaluate(v=>v.pause());
  }
  await page.setViewportSize({width:390,height:844});await page.waitForTimeout(100);
  ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'narrow layout has no horizontal overflow');
  ok(!errors.length&&!failures.length,'no script errors or missing same-origin assets');
  const manifest=await page.evaluate(async()=>await (await fetch('build.json')).json());
  if(process.env.EXPECTED_SHA){
   ok(manifest.source_commit===process.env.EXPECTED_SHA,'deployed source SHA matches tested commit');
   const hero=await page.request.get(new URL('media/browser-lab.png',base).href);
   ok(hero.status()===200&&(await hero.body()).length>10000,'README screenshot is publicly available');
  }
 }catch(e){checks.push({name:e.message,pass:false});await page.screenshot({path:out+'/failure.png',fullPage:true}).catch(()=>{});process.exitCode=1;}
 fs.writeFileSync(out+'/browser-acceptance.json',JSON.stringify({url:base,checks,errors,failures,accepted:checks.every(x=>x.pass)},null,2));
 console.log(JSON.stringify({checks,errors,failures},null,2));await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
