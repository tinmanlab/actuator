import {createPwmInspector} from './pwm-inspector.js';
/* Observations and diagrams only; numerical power/PWM and dyno data come from C++.
   A frozen-duty PWM reconstruction is explicitly not a switched live plant. */
export function createDashboard(){
 const $=id=>document.getElementById(id), colors=['#5ce4bb','#ffc971','#6aaeff','#dd90c5'];
 const state={history:[],wave:null,map:null,mapBusy:false,drawnAt:0};window.dashboardState=state;
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
  const last=state.history.at(-1);if(last)inspector.receive(packet.wave,last.r,last.s,packet.type);
  if(state.history.length>8000)state.history.splice(0,state.history.length-8000);
 }
 const inspector=createPwmInspector(plot);
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
  if(!state.map){plot('tn-scope',[{points:state.history.map(x=>[x.r[24]*60/(2*Math.PI),x.r[6]/6])}],'Gear-input torque [N·m]',{xunit:'RPM'});plot('eta-scope',[],'Independent dyno η [%]');}else mapPlots();
  $('dc-voltage').textContent=format(r[7],1)+' V';$('dc-current').textContent=format(p[9],3)+' A';
  $('diagram-reference').textContent=`Id* ${format(s[26])} / Iq* ${format(r[5])} A`;
  $('diagram-adc').textContent=`Raw ia ${format(s[8])} A · age ${format(s[6]*1e6,0)} μs`;
  $('diagram-dq').textContent=`Id ${format(s[2])} / Iq ${format(s[3])} A`;
  $('diagram-error').textContent=`ed ${format(s[26]-s[2])} / eq ${format(r[5]-s[3])} A`;
  $('diagram-regulator').textContent=s[25]?'Predictive + correction':'PI + feed-forward';
  $('diagram-voltage').textContent=`Vd ${format(s[4])} / Vq ${format(s[5])} V`;
  $('diagram-duty').textContent=r.slice(14,17).map(v=>format(v*100,1)+'%').join(' / ');
  $('foc-diagram').dataset.time=r[0];
  energy(state.history);state.time=r[0];
 }
 $('scope-window').onchange=()=>update(true);
 window.addEventListener('resize',()=>{clearTimeout(state.resize);state.resize=setTimeout(()=>{update(true);inspector.refresh();},150);});
 return {ingest,update};
}
