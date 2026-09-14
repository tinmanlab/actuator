// Supplemental acceptance: native graphs and the single reading route.
const {chromium}=require('@playwright/test');
const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/';
 const out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const checks=[],errors=[],missing=[];const report=()=>fs.writeFileSync(out+'/signals-acceptance.json',JSON.stringify({url:base,checks,errors,missing,accepted:checks.length>15&&checks.every(x=>x.pass)&&!errors.length&&!missing.length},null,2));
 const ok=(v,name)=>{checks.push({name,pass:!!v});report();if(!v)throw Error(name);};
 const browser=await chromium.launch({headless:true});
 const page=await browser.newPage({viewport:{width:1440,height:1050}});
 page.setDefaultTimeout(20000);page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400)missing.push(r.url());});
 const watchdog=setTimeout(()=>{checks.push({name:'signal test deadline',pass:false});report();process.exit(1);},90000);
 try{
  await page.goto(new URL('physics.html',base).href,{waitUntil:'networkidle'});
  await page.waitForFunction(()=>window.signalEvidence?.ready);
  ok(await page.locator('.signal-plot').count()===8,'eight native-data charts present');
  ok(await page.locator('#current-plot path').count()===3,'reference, true and sensed current shown');
  ok(await page.evaluate(()=>signalEvidence.thermal.winding_C>signalEvidence.thermal.case_C),'thermal ordering matches native data');
  ok(await page.evaluate(()=>signalEvidence.motoring.efficiency>0&&signalEvidence.motoring.efficiency<1),'qualified motoring result finite and bounded');
  await page.locator('#pwm').scrollIntoViewIfNeeded();
  await page.screenshot({path:out+'/signals-svpwm.png'});
  const old=await page.locator('#pwm-plot path').first().getAttribute('d');
  await page.selectOption('#pwm-phase','c');
  ok(await page.locator('#pwm-plot path').first().getAttribute('d')!==old,'phase selector changes real gate data');
  await page.selectOption('#pwm-signal','voltage');
  ok((await page.locator('#pwm-plot-legend').innerText()).includes('Line CA'),'line and phase-neutral voltage distinguished');
  await page.selectOption('#pwm-signal','current');
  ok(await page.locator('#pwm-plot path').count()===1,'current ripple selected without voltage overlay');
  await page.uncheck('#pwm-zoom');
  ok((await page.locator('#pwm-plot').textContent()).includes('200'),'four-period window available');
  await page.selectOption('#regen-signal','vbus_V');
  ok((await page.locator('#regen-plot').textContent()).includes('Voltage [V]'),'regeneration voltage uses its own units');
  await page.locator('#thermal').scrollIntoViewIfNeeded();await page.screenshot({path:out+'/signals-thermal.png'});
  const raw=await page.request.get(new URL('data/native-signals.zip',base).href);
  ok(raw.status()===200&&(await raw.body()).subarray(0,2).toString()==='PK','native raw evidence archive downloadable');
  const manifest=await (await page.request.get(new URL('data/signals-manifest.json',base).href)).json();
  ok(/^[a-f0-9]{64}$/.test(manifest.native_executable_sha256),'dataset bound to native executable hash');
  ok(Object.keys(manifest.files).includes('pwm_edges.csv'),'raw gate trace hash recorded');
  for(const width of [390,768,1440]){await page.setViewportSize({width,height:1000});await page.waitForTimeout(200);ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'signal page fits '+width+' px');}
  await page.goto(new URL('references.html',base).href,{waitUntil:'networkidle'});
  ok(await page.locator('.reference-entry').count()===14,'14 annotated primary references');
  for(const width of [390,768,1440]){await page.setViewportSize({width,height:1000});ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'references fit '+width+' px');}
  if(process.env.EXPECTED_SHA){await page.waitForFunction(()=>document.querySelector('[data-source]').href.includes(document.getElementById('guide-version').textContent.split(' ')[1]));ok((await page.locator('[data-source]').first().getAttribute('href')).includes(process.env.EXPECTED_SHA),'reference implementation links bind deployment');}
  ok(!errors.length&&!missing.length,'no page exceptions or missing assets');
 }catch(e){checks.push({name:e.message,pass:false});process.exitCode=1;await page.screenshot({path:out+'/signals-failure.png'}).catch(()=>{});}
 finally{report();console.log(JSON.stringify({checks,errors,missing},null,2));clearTimeout(watchdog);await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
