/* Observations and diagrams only; numerical power/PWM and dyno data come from C++.
   A frozen-duty PWM reconstruction is explicitly not a switched live plant. */
export function createDashboard(){
 const $=id=>document.getElementById(id), colors=['#5ce4bb','#ffc971','#6aaeff','#dd90c5'];
 const state={history:[],wave:null,map:null,mapBusy:false,drawnAt:0,pwmPlaying:true,pwmTimer:null,pwmPhaseUs:12.5,flowPhase:0,flowTimer:null};window.dashboardState=state;
 const format=(v,n=2)=>Number.isFinite(v)?v.toFixed(n):'—';
 function plot(id,series,unit,{range=null,yrange=null,dots=false,xunit='s'}={}){
  const canvas=$(id);if(!canvas)return;const rect=canvas.getBoundingClientRect();if(rect.width<1)return;
  const w=rect.width,h=rect.height,dpr=Math.min(devicePixelRatio,2);canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);
  const ctx=canvas.getContext('2d');ctx.scale(dpr,dpr);ctx.clearRect(0,0,w,h);const l=38,r=12,t=18,b=23;
  const points=series.flatMap(s=>s.points).filter(p=>p.every(Number.isFinite));
  ctx.font='9px ui-monospace,monospace';ctx.fillStyle='#9ab2c2';ctx.fillText(unit,3,11);
  if(!points.length){ctx.fillText('No qualified samples yet',l,h*.55);return;}
  let xmin=range?range[0]:Math.min(...points.map(p=>p[0])),xmax=range?range[1]:Math.max(...points.map(p=>p[0]));if(xmax<=xmin)xmax=xmin+1;
  let ymin=yrange?yrange[0]:Math.min(...points.map(p=>p[1])),ymax=yrange?yrange[1]:Math.max(...points.map(p=>p[1]));
  if(!yrange){const pad=Math.max(.02,(ymax-ymin)*.1);ymin-=pad;ymax+=pad;}
  const X=x=>l+(x-xmin)/(xmax-xmin)*(w-l-r),Y=y=>h-b-(y-ymin)/(ymax-ymin)*(h-t-b);
  ctx.strokeStyle='#29414f';ctx.lineWidth=.6;
  for(let i=0;i<=2;i++){const y=ymin+(ymax-ymin)*i/2;ctx.beginPath();ctx.moveTo(l,Y(y));ctx.lineTo(w-r,Y(y));ctx.stroke();ctx.fillText(Number(y.toPrecision(3)),2,Y(y)+3);}
  for(let i=0;i<=2;i++){const x=xmin+(xmax-xmin)*i/2;ctx.textAlign=i===0?'left':i===2?'right':'center';ctx.fillText(Number(x.toPrecision(4))+(i===2?' '+xunit:''),X(x),h-6);}ctx.textAlign='left';
  ctx.save();ctx.beginPath();ctx.rect(l,t,w-l-r,h-t-b);ctx.clip();
  series.forEach((s,k)=>{ctx.strokeStyle=colors[k%4];ctx.fillStyle=colors[k%4];ctx.lineWidth=1.5;ctx.setLineDash(k===2?[3,2]:[]);ctx.beginPath();let old=null;
   for(const p of s.points){if(!p.every(Number.isFinite)){old=null;continue;}if(old){if(s.step)ctx.lineTo(X(p[0]),Y(old[1]));ctx.lineTo(X(p[0]),Y(p[1]));}else ctx.moveTo(X(p[0]),Y(p[1]));old=p;
   }ctx.stroke();if(dots)for(const p of s.points)if(p.every(Number.isFinite)){ctx.beginPath();ctx.arc(X(p[0]),Y(p[1]),2.5,0,7);ctx.fill();}});ctx.restore();
  canvas.dataset.samples=String(points.length);canvas.dataset.xmin=xmin;canvas.dataset.xmax=xmax;
 }
 function ingest(packet){
  const rows=packet.rows||(packet.row?[packet.row]:[]),sig=packet.signals||(packet.signal?[packet.signal]:[]),powers=packet.powers||(packet.power?[packet.power]:[]);
  if(!rows.length)return;
  if(state.history.length&&rows[0][0]<=state.history.at(-1).r[0])state.history=[];
  if(packet.wave)state.wave=packet.wave;
  rows.forEach((r,k)=>{const s=sig[k],p=powers[k];if(s&&p&&r[0]===s[0]&&r[0]===p[0])state.history.push({r,s,p});});
  if(state.history.length>8000)state.history.splice(0,state.history.length-8000);
 }
 const svgNS='http://www.w3.org/2000/svg';
 function element(tag,attrs,text){const e=document.createElementNS(svgNS,tag);for(const[k,v]of Object.entries(attrs))e.setAttribute(k,v);if(text)e.textContent=text;return e;}
 const circuit=$('bridge-circuit');
 const focDiagram=$('foc-diagram');
 if(focDiagram){
  const pulse=element('circle',{id:'foc-flow-pulse',cx:12,cy:35,r:4,class:'flow-pulse'});
  const returnPath=element('path',{id:'foc-feedback-path',d:'M610 61H18',class:'feedback-path',fill:'none'});
  const feedbackPulse=element('circle',{id:'foc-feedback-pulse',cx:610,cy:61,r:3,class:'flow-pulse feedback-pulse'});
  focDiagram.append(returnPath,pulse,feedbackPulse);
  state.flowTimer=setInterval(()=>{
   if(document.hidden||!window.labState?.running)return;
   state.flowPhase=(state.flowPhase+.045)%1;
   pulse.setAttribute('cx',12+598*state.flowPhase);
   feedbackPulse.setAttribute('cx',610-592*state.flowPhase);
   focDiagram.dataset.flowPhase=state.flowPhase.toFixed(3);
  },80);
 }
 for(let phase=0;phase<3;phase++){
  const x=52+phase*78;
  circuit.append(element('path',{d:`M${x} 13V119 M${x} 67h27`,fill:'none',stroke:colors[phase]}));
  for(let side=0;side<2;side++){const y=side?84:27,id=2*phase+side;circuit.append(element('rect',{id:'circuit-gate-'+id,x:x-20,y,width:40,height:24,rx:3,class:'gate'}),element('text',{x,y:y+16,'text-anchor':'middle',fill:'#d3e7ed'},'ABC'[phase]+(side?'L':'H')));}
  circuit.append(element('text',{x:x+29,y:71,fill:colors[phase]},'UVW'[phase]));
 }
 function drawGateCursor(us){
  const canvas=$('gate-scope'),rect=canvas.getBoundingClientRect();if(rect.width<1||!Number.isFinite(us))return;
  const dpr=canvas.width/rect.width||1,ctx=canvas.getContext('2d'),l=38,r=12,x=l+Math.max(0,Math.min(1,us/50))*(rect.width-l-r);
  ctx.save();ctx.setTransform(dpr,0,0,dpr,0,0);ctx.strokeStyle='#f5fbff';ctx.lineWidth=1;ctx.setLineDash([3,2]);ctx.beginPath();ctx.moveTo(x,18);ctx.lineTo(x,rect.height-23);ctx.stroke();ctx.restore();
  canvas.dataset.cursorUs=us.toFixed(3);
 }
 function setPwmPlaying(value){
  state.pwmPlaying=!!value;
  const button=$('pwm-replay');if(button){button.textContent=state.pwmPlaying?'Pause PWM replay':'Play PWM replay';button.setAttribute('aria-pressed',String(state.pwmPlaying));}
  const status=$('pwm-replay-status');if(status)status.textContent=state.pwmPlaying?'Stepping through native gate-change events in one reconstructed 50 μs period':'Paused for manual inspection';
 }
 function renderGateSample(sample,rows){
  state.pwmPhaseUs=sample[0]*1e6;
  for(let k=0;k<6;k++)$('circuit-gate-'+k).classList.toggle('on',!!sample[4+k]);
  $('pwm-time').textContent=format(state.pwmPhaseUs,2)+' μs';
  const mode=$('pwm-interface').value==='3';
  $('pwm-owner').textContent=mode?'3PWM requests A/B/C → driver adds complementary gates and 150 ns dead time.':'6PWM: timer supplies AH/AL (shown), BH/BL, CH/CL with 150 ns dead time.';
  const cols=mode?[1,2,3]:[4,5,10];plot('gate-scope',cols.map(c=>({step:true,points:rows.map(a=>[a[0]*1e6,a[c]])})),'Logic 0/1'+(mode?' · A/B/C':' · AH/AL/carrier'),{range:[0,50],yrange:[-.1,1.1],xunit:'μs'});
  drawGateCursor(state.pwmPhaseUs);
  circuit.dataset.time=state.wave.time;circuit.dataset.reconstructed='true';circuit.dataset.sampleUs=state.pwmPhaseUs.toFixed(4);
 }
 function gates(requestedUs=state.pwmPhaseUs){
  if(!state.wave||!state.wave.rows.length)return;
  const rows=state.wave.rows,phaseUs=Number.isFinite(requestedUs)?requestedUs:Number($('pwm-phase').value),requested=phaseUs*1e-6;
  let sample=rows[0];for(const a of rows){if(a[0]>requested)break;sample=a;}
  renderGateSample(sample,rows);
 }
 const phaseCursor=document.querySelector('.phase-cursor');
 if(phaseCursor){
  const controls=document.createElement('div');controls.className='pwm-replay-controls';
  const replay=document.createElement('button');replay.type='button';replay.id='pwm-replay';replay.setAttribute('aria-pressed','true');
  const replayStatus=document.createElement('small');replayStatus.id='pwm-replay-status';
  controls.append(replay,replayStatus);phaseCursor.insertAdjacentElement('afterend',controls);
  replay.onclick=()=>setPwmPlaying(!state.pwmPlaying);
  setPwmPlaying(true);
  const gateSignature=row=>row.slice(4,10).map(v=>v?'1':'0').join('');
  state.pwmTimer=setInterval(()=>{
   if(!state.pwmPlaying||document.hidden||!state.wave?.rows?.length)return;
   const slider=$('pwm-phase'),rows=state.wave.rows,current=state.pwmPhaseUs*1e-6;
   let sample=rows[0];for(const row of rows){if(row[0]>current+1e-15)break;sample=row;}
   const signature=gateSignature(sample);
   let next=rows.find(row=>row[0]>current+1e-12&&gateSignature(row)!==signature);
   if(!next)next=rows.find(row=>gateSignature(row)!==signature);
   if(next){slider.value=String(Math.min(49.999,next[0]*1e6));renderGateSample(next,rows);}
   else{const nextUs=(state.pwmPhaseUs+1.25)%50;slider.value=String(nextUs);gates(nextUs);}
  },220);
 }
 function energy(history){
  const end=history.at(-1).r[0],a=history.filter(x=>x.r[0]>=end-.2),mean=k=>a.reduce((n,x)=>n+x.p[k],0)/a.length;
  const pin=mean(1),pout=mean(7),storage=mean(10),fault=a.some(x=>x.r[12]);
  const eta=a.length>=180&&pin>.05&&pout>.01&&!fault&&Math.abs(storage)<.05*Math.max(1,Math.abs(pin))&&pout<=pin?pout/pin:null;
  $('live-efficiency').textContent=eta===null?'Live η: — (stall / transient / power direction)':'Live η: '+format(100*eta,1)+'% · external-load boundary';
  state.liveEfficiency=eta;
  const losses=[['Copper',mean(4)],['Inverter',mean(3)],['Friction',mean(5)],['Gear',mean(6)]];const total=losses.reduce((s,a)=>s+Math.max(0,a[1]),0);
  $('loss-bars').innerHTML=losses.map(([name,v],k)=>`<div class="loss-row"><span>${name}</span><div class="track"><i style="width:${total?100*Math.max(0,v)/total:0}%;background:${colors[k]}"></i></div><output>${format(v,2)} W</output></div>`).join('');
  $('power-residual').textContent=`DC ${format(pin)} W · residual ${format(mean(11),3)} W`;
 }
 function mapPlots(){
  if(!state.map)return;
  const series=(col,qualified=false)=>[2,5,10,15].map(q=>({points:state.map.filter(r=>r[1]===q).map(r=>[r[0],qualified?(r[6]?r[col]*100:NaN):r[col]])}));
  plot('tn-scope',series(2),'Dyno shaft [N·m] · 2/5/10/15 A',{range:[0,3600],dots:true,xunit:'RPM'});
  plot('eta-scope',series(5,true),'Dyno η [%]',{range:[0,3600],yrange:[0,100],dots:true,xunit:'RPM'});
 }
 let mapWorker;
 $('calculate-map').onclick=()=>{
  if(state.mapBusy)return;state.mapBusy=true;$('calculate-map').disabled=true;$('map-status').textContent='Calculating independent C++ dyno…';
  const request=()=>mapWorker.postMessage({type:'run',id:1,parameters:[2,4,600,.015,10000,150,25,.0002]});
  if(!mapWorker){mapWorker=new Worker('experiment-worker.js');mapWorker.onmessage=({data:d})=>{
   if(d.type==='ready'){request();return;}
   if(d.type==='result'){state.map=Array.from({length:d.tables[3].length/48},(_,k)=>Array.from(d.tables[3].slice(k*48,k*48+48)));$('map-status').textContent=`Independent 25°C dyno · ${state.map.filter(r=>r[6]).length}/28 qualified · not live joint`;mapPlots();}
   if(d.type==='error')$('map-status').textContent='Dyno unavailable: '+d.message;
   state.mapBusy=false;$('calculate-map').disabled=false;
  };mapWorker.onerror=e=>{state.mapBusy=false;$('calculate-map').disabled=false;$('map-status').textContent='Dyno unavailable: '+e.message;};}else request();
 };
 function update(force=false){
  if(!state.history.length)return;if(!force&&performance.now()-state.drawnAt<100)return;state.drawnAt=performance.now();
  const last=state.history.at(-1),{r,s,p}=last,end=r[0],win=+$('scope-window').value;
  const visible=state.history.filter(x=>x.r[0]>=end-win),range=[Math.max(0,end-win)*1000,Math.max(win,end)*1000];
  const line=(property,k)=>({points:visible.map(x=>[x.r[0]*1000,x[property][k]])});
  plot('phase-scope',[11,12,13].map(k=>line('s',k)),'Current [A]',{range,xunit:'ms'});
  plot('dq-scope',[line('s',2),line('s',3),line('r',5)],'Current [A]',{range,xunit:'ms'});
  plot('duty-scope',[14,15,16].map(k=>({...line('r',k),step:true})),'Duty',{range,yrange:[0,1],xunit:'ms'});
  plot('sensor-scope',[line('s',11),line('s',17),{...line('s',8),step:true}],'Current [A]',{range,xunit:'ms'});
  plot('temperature-scope',[{points:state.history.map(x=>[x.r[0],x.r[9]])},{points:state.history.map(x=>[x.r[0],x.p[12]])},{points:state.history.map(x=>[x.r[0],x.r[10]])}],'Temperature [°C]');
  if(!state.map){plot('tn-scope',[{points:state.history.map(x=>[x.r[24]*60/(2*Math.PI),x.r[6]/6])}],'Gear-input torque [N·m]',{xunit:'motor RPM'});plot('eta-scope',[],'Independent dyno η [%]');}else mapPlots();
  $('dc-voltage').textContent=format(r[7],1)+' V';$('dc-current').textContent=format(p[9],3)+' A';
  const duty=r.slice(14,17),mean=duty.reduce((a,b)=>a+b,0)/3;
  const alpha=(duty[0]-mean)*r[7],beta=(duty[0]+2*duty[1]-3*mean)*r[7]/Math.sqrt(3),scale=44/(r[7]*2/3||1);
  $('duty-vector').setAttribute('d',`M60 52L${60+alpha*scale} ${52-beta*scale}`);
  energy(state.history);gates();state.time=r[0];
 }
 $('pwm-phase').oninput=()=>{state.pwmPhaseUs=Number($('pwm-phase').value);setPwmPlaying(false);gates(state.pwmPhaseUs);};$('pwm-interface').onchange=()=>gates(state.pwmPhaseUs);$('scope-window').onchange=()=>update(true);
 window.addEventListener('resize',()=>{clearTimeout(state.resize);state.resize=setTimeout(()=>update(true),150);});
 return {ingest,update};
}
