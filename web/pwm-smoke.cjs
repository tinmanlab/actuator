/* User-visible bug regressions. Real WASM data and DOM, no synthetic screenshots. */
const {chromium}=require('@playwright/test');const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/',out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const checks=[],errors=[],requests=[];let stage='load';
 const save=()=>fs.writeFileSync(out+'/pwm-acceptance.json',JSON.stringify({url:base,stage,checks,errors,requests,accepted:stage==='complete'&&checks.every(c=>c.pass)&&!errors.length&&!requests.length},null,2));
 const ok=(v,name)=>{checks.push({name,pass:!!v});save();console.log(v?'PASS':'FAIL',name);if(!v)throw Error(name);};
 const watchdog=setTimeout(()=>{checks.push({name:'timeout at '+stage,pass:false});save();process.exit(1);},180000);
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1640,height:1100}});page.setDefaultTimeout(20000);
 const observe=p=>{p.on('pageerror',e=>errors.push(e.message));p.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))requests.push(r.status()+' '+r.url());});};observe(page);
 try{
  await page.goto(base,{waitUntil:'networkidle'});await page.waitForFunction(()=>window.labState?.snapshot?.[0]>.8&&window.pwmInspectorState?.playing,null,{timeout:60000});
  ok(await page.locator('#foc-diagram .diagram-blocks li').count()===5,'current-loop sequence is a readable five-block diagram');
  ok(await page.locator('.diagram-blocks strong').evaluateAll(es=>es.every(e=>parseFloat(getComputedStyle(e).fontSize)>=14)),'loop block titles are at least 14 CSS pixels, not scaled SVG text');
  const at=await page.evaluate(()=>pwmInspectorState.phase);await page.waitForTimeout(250);
  ok(await page.evaluate(x=>pwmInspectorState.phase!==x,at),'PWM cursor advances automatically while the joint runs');
  await page.locator('#load').evaluate(e=>{e.value='2';e.dispatchEvent(new Event('input',{bubbles:true}));});
  const t=await page.evaluate(()=>labState.snapshot[0]);await page.waitForFunction(t=>labState.snapshot[0]>t+.8,t);
  await page.click('#run');await page.waitForTimeout(150);
  ok(await page.evaluate(()=>!pwmInspectorState.playing),'pausing the joint also pauses automatic PWM animation');
  await page.click('#pwm-capture');
  const frozen=await page.evaluate(()=>JSON.stringify(labState.snapshot)),mean=await page.locator('#duty-vector').getAttribute('d');
  ok(await page.evaluate(()=>pwmInspectorState.capture.time===labState.snapshot[0]),'Capture latest binds the native reconstruction to the paused scene timestamp');
  stage='switching edges';
  const before=await page.evaluate(()=>pwmInspectorState.sample.slice(4,10).join(''));await page.click('#pwm-next');
  ok(await page.evaluate(x=>pwmInspectorState.sample.slice(4,10).join('')!==x,before),'Next edge changes native gate bits, not just the label');
  ok(await page.evaluate(()=>pwmInspectorState.decoded.kind==='deadtime'),'edge inspection exposes actual native dead time');
  ok(await page.locator('#switch-vector').evaluate(e=>e.style.visibility==='hidden'),'no ideal switching vector is fabricated during dead time');
  ok(await page.evaluate(()=>pwmInspectorState.decoded.paths.some(p=>p.endsWith('diode'))),'captured nonzero current shows a freewheel diode path');
  ok(await page.evaluate(()=>[0,1,2,3,4,5].every(k=>document.getElementById('circuit-gate-'+k).classList.contains('on')===!!pwmInspectorState.sample[k+4])),'all six visual switches match the selected native sample');
  await page.click('#pwm-capture');await page.click('#pwm-play');await page.waitForTimeout(100);
  const token=await page.locator('#flow-token-0').evaluate(e=>[e.getAttribute('cx'),e.getAttribute('cy')].join());await page.waitForTimeout(140);
  ok(await page.locator('#flow-token-0').evaluate((e,p)=>[e.getAttribute('cx'),e.getAttribute('cy')].join()!==p,token),'conventional-current direction dots move during slow playback');
  ok(await page.locator('#duty-vector').getAttribute('d')===mean,'mean command vector is stationary during a frozen-duty replay');
  ok(await page.evaluate(x=>JSON.stringify(labState.snapshot)===x,frozen),'manual PWM playback does not advance paused motor physics');
  await page.click('#pwm-play');const phase=await page.evaluate(()=>pwmInspectorState.phase);await page.waitForTimeout(160);
  ok(await page.evaluate(x=>pwmInspectorState.phase===x,phase),'Pause cycle freezes the presentation clock');
  await page.selectOption('#pwm-interface','3');ok((await page.locator('#pwm-owner').textContent()).startsWith('3PWM requests'),'3PWM input ownership is labeled without changing the motor');
  await page.selectOption('#pwm-interface','6');await page.locator('#pwm-phase').evaluate(e=>{e.value='25';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok(await page.evaluate(()=>pwmInspectorState.decoded.kind==='zero'&&pwmInspectorState.decoded.label==='000'),'mid-cycle zero vector is visible and named 000');
  stage='enlarged inspection';
  const y=await page.locator('#viewport').evaluate(e=>e.getBoundingClientRect().top+scrollY);await page.click('#pwm-expand');
  ok(await page.locator('#pwm-dialog').evaluate(e=>e.open)&&await page.locator('#bridge-circuit').count()===1,'expanded circuit reuses the same DOM and captured sample');
  await page.screenshot({path:out+'/pwm-expanded.png'});await page.keyboard.press('Escape');await page.waitForFunction(()=>!document.getElementById('pwm-dialog').open);
  ok(await page.locator('#pwm-expand').evaluate(e=>document.activeElement===e),'Escape closes the circuit dialog and restores keyboard focus');
  ok(Math.abs(await page.locator('#viewport').evaluate(e=>e.getBoundingClientRect().top+scrollY)-y)<.5,'expanded circuit restores the unchanged viewport layout');
  await page.click('#diagram-expand');ok(await page.locator('#diagram-dialog').evaluate(e=>e.open),'current-loop diagram has an enlarged view');await page.keyboard.press('Escape');
  await page.click('[data-live-stage="foc"]');
  for(const width of [1640,768,390]){
   await page.setViewportSize({width,height:1100});await page.evaluate(()=>{document.activeElement?.blur();document.documentElement.style.scrollBehavior='auto';scrollTo({top:0,behavior:'instant'});});await page.waitForTimeout(250);
   ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'diagram layout fits '+width+' pixels');
   await page.screenshot({path:out+'/pwm-screen-'+width+'.png'});
  }
  stage='fault and reduced motion';
  await page.click('#fault');await page.click('#step');await page.waitForFunction(()=>labState.snapshot?.[12]!==0);
  ok(await page.evaluate(()=>!pwmInspectorState.capture.enabled&&pwmInspectorState.decoded.kind==='off'&&!pwmInspectorState.playing),'driver fault replaces the captured energized circuit immediately');
  const reduced=await browser.newPage({reducedMotion:'reduce'});observe(reduced);await reduced.goto(base,{waitUntil:'networkidle'});
  await reduced.waitForFunction(()=>window.labState?.snapshot?.[0]>.4&&window.pwmInspectorState?.capture,null,{timeout:60000});
  ok(await reduced.evaluate(()=>!pwmInspectorState.playing),'reduced-motion preference disables automatic PWM motion');await reduced.close();
  if(process.env.EXPECTED_SHA){const m=await(await page.request.get(new URL('build.json',base).href)).json();ok(m.source_commit===process.env.EXPECTED_SHA,'public source matches the merged commit');}
  ok(!errors.length&&!requests.length,'no script errors or missing resources');stage='complete';
 }catch(e){checks.push({name:e.message,pass:false});await page.screenshot({path:out+'/pwm-failure.png',fullPage:true}).catch(()=>{});process.exitCode=1;}
 save();clearTimeout(watchdog);await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
