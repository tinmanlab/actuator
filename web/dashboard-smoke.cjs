const {chromium}=require('@playwright/test');const fs=require('fs');
(async()=>{
 const base=process.env.LAB_URL||'http://127.0.0.1:8765/',out=process.env.LAB_EVIDENCE||'../results/browser';fs.mkdirSync(out,{recursive:true});
 const checks=[],errors=[],requests=[];let stage='load';
 const save=()=>fs.writeFileSync(out+'/dashboard-acceptance.json',JSON.stringify({url:base,stage,checks,errors,requests,accepted:stage==='complete'&&checks.every(x=>x.pass)&&!errors.length&&!requests.length},null,2));
 const ok=(v,n)=>{checks.push({name:n,pass:!!v});save();if(!v)throw Error(n);};
 const browser=await chromium.launch({headless:true,args:['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader']});
 const page=await browser.newPage({viewport:{width:1640,height:1040}});page.setDefaultTimeout(25000);
 page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400&&!r.url().endsWith('favicon.ico'))requests.push(r.status()+' '+r.url());});
 try{
  await page.goto(base,{waitUntil:'networkidle'});await page.waitForFunction(()=>window.dashboardState?.time>.8&&window.labState?.modelLoaded);
  ok(await page.locator('body').evaluate(e=>e.classList.contains('instrument-dashboard')),'main URL uses the approved dark instrument workspace');
  // Guard against more-specific light-theme rules leaking into the dark editor.
  const contrasts=await page.evaluate(()=>{
   const rgb=s=>(s.match(/[\d.]+/g)||[]).map(Number);
   const lum=v=>rgb(v).slice(0,3).map(x=>{x/=255;return x<=.04045?x/12.92:((x+.055)/1.055)**2.4;}).reduce((a,x,k)=>a+x*[.2126,.7152,.0722][k],0);
   return ['#kp','#kd','#mode','.controls label','.visual-panel .model-note'].map(selector=>{
    const el=document.querySelector(selector),foreground=getComputedStyle(el).color;let parent=el,bg;
    do{bg=getComputedStyle(parent).backgroundColor;parent=parent.parentElement;}while(parent&&(rgb(bg)[3]??1)===0);
    const a=lum(foreground),b=lum(bg);return {selector,foreground,background:bg,ratio:(Math.max(a,b)+.05)/(Math.min(a,b)+.05)};
   });
  });
  fs.writeFileSync(out+'/dashboard-contrast.json',JSON.stringify(contrasts,null,2));
  ok(contrasts.every(c=>c.ratio>=4.5),'command values, labels and model note have readable dark-theme contrast');
  ok(await page.evaluate(()=>dashboardState.history.length>=500&&dashboardState.history.every(x=>x.r[0]===x.s[0]&&x.r[0]===x.p[0])),'current and energy histories match the scene clock');
  for(const id of ['phase-scope','dq-scope','duty-scope','sensor-scope','temperature-scope','tn-scope'])ok(await page.locator('#'+id).evaluate(e=>+e.dataset.samples>5),id+' contains numerical samples');
  ok(await page.locator('#bridge-circuit .gate').count()===6,'three-phase bridge has six switch nodes');
  await page.click('#run');await page.waitForTimeout(150);
  const frozen=await page.evaluate(()=>JSON.stringify(labState.snapshot));
  await page.selectOption('#pwm-interface','3');await page.locator('#pwm-phase').evaluate(e=>{e.value='7.5';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok((await page.locator('#pwm-owner').textContent()).startsWith('3PWM requests'),'3PWM explains driver-owned complementary gates');
  await page.selectOption('#pwm-interface','6');
  ok(await page.evaluate(x=>JSON.stringify(labState.snapshot)===x,frozen),'diagram interface does not alter frozen joint physics');
  stage='stable pending status';
  const y0=await page.locator('#viewport').evaluate(e=>e.getBoundingClientRect().top+scrollY);
  await page.locator('#target').evaluate(e=>{e.value='.71';e.dispatchEvent(new Event('input',{bubbles:true}));});
  ok(await page.locator('#pipeline-pending').isVisible(),'paused input has an explicit pending chip');
  const y1=await page.locator('#viewport').evaluate(e=>e.getBoundingClientRect().top+scrollY);
  await page.click('#step');await page.waitForFunction(()=>Math.abs(labState.signal[19]-.71)<1e-5);await page.waitForTimeout(100);
  const y2=await page.locator('#viewport').evaluate(e=>e.getBoundingClientRect().top+scrollY);
  ok(Math.abs(y0-y1)<.5&&Math.abs(y0-y2)<.5,'pending shown and cleared causes zero viewport layout shift');
  ok(await page.locator('#pipeline-pending').isHidden(),'applied command clears pending state');
  await page.click('#run');
  const shifts=await page.evaluate(async()=>{const e=document.getElementById('viewport'),initial=e.getBoundingClientRect().top+scrollY;let max=0;for(let k=0;k<35;k++){const input=document.getElementById('target');input.value=String(.3+.1*Math.sin(k));input.dispatchEvent(new Event('input',{bubbles:true}));await new Promise(r=>setTimeout(r,20));max=Math.max(max,Math.abs(e.getBoundingClientRect().top+scrollY-initial));}return max;});
  ok(shifts<.5,'continuous running slider edits do not shift the viewport');
  stage='map';await page.click('#calculate-map');await page.waitForFunction(()=>dashboardState.map?.length===28&&!dashboardState.mapBusy,null,{timeout:90000});
  ok(await page.locator('#tn-scope').getAttribute('data-xmax')==='3600'&&await page.locator('#eta-scope').getAttribute('data-xmax')==='3600','inline torque and efficiency maps keep the same RPM domain');
  ok(await page.evaluate(()=>dashboardState.map.filter(r=>r[0]===0).every(r=>r[5]===-1)),'unqualified stall efficiencies are not fabricated');
  await page.click('#reset');await page.waitForFunction(()=>labState.snapshot?.[0]===0&&!labState.running);
  await page.click('#run');await page.waitForFunction(()=>labState.snapshot?.[0]>.8);await page.click('#run');await page.waitForTimeout(150);
  ok(await page.locator('#loss-bars .loss-row').count()===4,'modeled loss channels are explicit');
  const scene=await(await page.request.get(new URL('scene.json',base).href)).json();const names=[];(function visit(n){names.push(n.a.name);n.children.forEach(visit);})(scene.world);
  ok(names.includes('dc_supply')&&names.includes('motor_terminal')&&names.includes('dc_return_3'),'scene includes DC supply and connected power harness');
  stage='visual review';
  for(const width of [1640,1440,768,390]){
   await page.setViewportSize({width,height:1040});
   await page.evaluate(()=>{document.activeElement?.blur();document.documentElement.style.scrollBehavior='auto';window.scrollTo({top:0,behavior:'instant'});});
   await page.waitForFunction(()=>window.scrollY===0&&Math.abs(document.querySelector('header').getBoundingClientRect().top)<1);
   await page.waitForTimeout(400);
   ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'dashboard fits '+width+' px');
   await page.screenshot({path:out+'/dashboard-screen-'+width+'.png',fullPage:false,animations:'disabled'});
   await page.screenshot({path:out+'/dashboard-'+width+'.png',fullPage:true,animations:'disabled'});
  }
  if(process.env.EXPECTED_SHA){const m=await(await page.request.get(new URL('build.json',base).href)).json();ok(m.source_commit===process.env.EXPECTED_SHA,'dashboard source matches deployed main');}
  ok(!errors.length&&!requests.length,'no script errors or missing assets');stage='complete';
 }catch(e){checks.push({name:e.message,pass:false});await page.screenshot({path:out+'/dashboard-failure.png',fullPage:true}).catch(()=>{});process.exitCode=1;}
 save();console.log(JSON.stringify({stage,checks,errors,requests}));await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
