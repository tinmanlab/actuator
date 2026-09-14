/* All numerical data comes from experiments.cpp/WASM. Drawing and replay only. */
(() => {
 const $=id=>document.getElementById(id),ids=['iq','rpm','noise','bandwidth','deadtime','ambient','friction'];
 const state={ready:false,busy:false,kind:0,tables:null,parameters:null,id:0,replay:null};
 window.powertrainState=state;
 const worker=new Worker('experiment-worker.js');
 const colors=['#17654e','#a76a22','#476ca1','#a04d73'];
 const fields=['time_s','iq_ref_A','actuation_angle_rad','id_true_A','iq_true_A','ia_true_A','ia_analog_A','ia_adc_raw_A','id_observed_A','iq_observed_A','vd_V','vq_V','valpha_V','vbeta_V','va_V','vb_V','vc_V','duty_a','duty_b','duty_c','vbus_V','target_motor_rpm','shaft_torque_Nm','winding_C','fet_C','AH','AL','BH','BL','CH','CL','dc_W','ac_W','inverter_loss_W','shaft_W','copper_W','friction_W','gate_enabled','state','fault','plant_angle_rad','current_age_s','current_limit_A','voltage_saturated','ib_true_A','ic_true_A','carrier','interval_s'];
 const mapFields=['motor_rpm','iq_ref_A','shaft_torque_Nm','dc_W','shaft_W','efficiency_or_minus_one','qualified','iq_mean_A','iq_error_A','voltage_saturation_fraction','fault','copper_W','inverter_W','friction_W','stored_energy_rate_W','numerical_residual_W','winding_C'];
 const fmt=(x,n=3)=>Number(x).toFixed(n);
 const parameters=()=>[state.kind,...ids.map(id=>Number($(id).value))];
 const same=(a,b)=>a&&JSON.stringify(a)===JSON.stringify(b);
 const note=(text,stale=false)=>{$('exp-status').textContent=text;$('exp-status').classList.toggle('stale',stale);};
 function stopReplay(){clearInterval(state.replay);state.replay=null;$('replay').textContent='Replay captured samples';}
 function markStale(){stopReplay();note('Settings changed. Calculate to replace the previous result; displayed traces have not been recalculated.',true);}
 ids.forEach(id=>$(id).addEventListener('input',markStale));
 function selectKind(kind){
  stopReplay();state.kind=kind;state.tables=null;state.parameters=null;$('iq').min=kind===1?'0':'-15';
  document.querySelectorAll('[data-kind]').forEach(b=>b.setAttribute('aria-pressed',String(+b.dataset.kind===kind)));
  ['electrical-panel','thermal-panel','map-panel'].forEach((id,i)=>$(id).hidden=i!==kind);
  ids.forEach(id=>$(id).disabled=(kind===1&&!['iq','ambient'].includes(id))||(kind===2&&['iq','rpm'].includes(id)));
  document.querySelectorAll('.signal-plot,.signal-legend').forEach(e=>e.replaceChildren());
  $('exp-summary').textContent='';$('export-exp').disabled=true;
  $('fixture-note').textContent=[
   '120 ms switched current loop, 20 kHz PWM. An ideal dynamometer imposes speed; not free acceleration.',
   '600 physical seconds at prescribed current. Noise, speed, dead time and drag are not used. Protection bypassed; no switching-energy heat.',
   'Fixed 28-point motor-shaft grid, 0–3600 RPM and 2/5/10/15 A. Noise, filter, averaged dead-time loss, ambient and rotor drag remain configurable.'
  ][kind];note('Choose settings, then calculate. No substitute curves are generated.');
 }
 for(const b of document.querySelectorAll('[data-kind]'))b.onclick=()=>selectKind(+b.dataset.kind);
 function busy(value){state.busy=value;$('calculate').disabled=value||!state.ready;document.querySelectorAll('[data-kind]').forEach(b=>b.disabled=value);}
 $('parameters').onsubmit=e=>{e.preventDefault();if(!state.ready||state.busy||!$('parameters').reportValidity())return;stopReplay();busy(true);$('exp-error').hidden=true;note('Calculating native C++ model in a Worker…');worker.postMessage({type:'run',id:++state.id,parameters:parameters()});};
 function fail(message){busy(false);$('exp-error').hidden=false;$('exp-error').textContent=message;note('Calculation unavailable. Any retained result is from the last successful run.',true);}
 worker.onerror=e=>{state.ready=false;fail(e.message||'WASM Worker failed');};
 worker.onmessage=({data:d})=>{
  if(d.type==='ready'){state.ready=true;busy(false);$('parameters').requestSubmit();return;}
  if(d.type==='error'){fail(d.message);return;}
  if(d.type!=='result'||d.id!==state.id)return;
  state.tables=d.tables.map(a=>Array.from({length:a.length/48},(_,r)=>Array.from(a.slice(r*48,r*48+48))));
  state.parameters=d.parameters;busy(false);$('export-exp').disabled=false;state.completed=(state.completed||0)+1;
  if(state.kind===0){$('sample').max=state.tables[1].length-1;$('sample').value='0';}
  draw();if(same(parameters(),d.parameters))note('Calculated locally from C++ / WASM. Change inputs to run another experiment.');else markStale();
 };
 const ns='http://www.w3.org/2000/svg';
 function svgNode(tag,a={},text){const n=document.createElementNS(ns,tag);for(const[k,v]of Object.entries(a))n.setAttribute(k,v);if(text!==undefined)n.textContent=text;return n;}
 function chart(id,series,xlabel,ylabel,windowRange){
  const svg=$(id),w=Math.max(300,svg.clientWidth),h=265,l=60,t=22,b=45,right=18;
  svg.replaceChildren();svg.setAttribute('viewBox',`0 0 ${w} ${h}`);
  let points=series.flatMap(s=>s.points).filter(p=>p.every(Number.isFinite));
  const legend=$(id+'-legend');legend.replaceChildren();
  if(!points.length){svg.append(svgNode('text',{x:l,y:100},'No qualified points for this condition'));return;}
  let xmin=windowRange?windowRange[0]:Math.min(...points.map(p=>p[0])),xmax=windowRange?windowRange[1]:Math.max(...points.map(p=>p[0]));
  if(xmax===xmin)xmax=xmin+1;const visible=points.filter(p=>p[0]>=xmin&&p[0]<=xmax);if(visible.length)points=visible;
  let ymin=Math.min(...points.map(p=>p[1])),ymax=Math.max(...points.map(p=>p[1]));const dy=Math.max(.001,(ymax-ymin)*.08);ymin-=dy;ymax+=dy;
  const X=x=>l+(x-xmin)/(xmax-xmin)*(w-l-right),Y=y=>h-b-(y-ymin)/(ymax-ymin)*(h-t-b);
  svg.append(svgNode('desc',{},`${ylabel} versus ${xlabel}. ${series.map(s=>s.name).join('; ')}. Native WASM computation; missing efficiency points are gaps.`));
  for(let i=0;i<=4;i++){let v=ymin+(ymax-ymin)*i/4,y=Y(v);svg.append(svgNode('line',{x1:l,x2:w-right,y1:y,y2:y,stroke:'#dce4db'}),svgNode('text',{x:l-7,y:y+4,'text-anchor':'end'},Number(v.toPrecision(3))));}
  for(let i=0;i<=2;i++){let v=xmin+(xmax-xmin)*i/2;svg.append(svgNode('text',{x:X(v),y:h-b+20,'text-anchor':'middle'},Number(v.toPrecision(4))));}
  svg.append(svgNode('text',{x:l,y:13},ylabel),svgNode('text',{x:(l+w-right)/2,y:h-6,'text-anchor':'middle'},xlabel));
  const clip=svgNode('clipPath',{id:id+'-clip'});clip.append(svgNode('rect',{x:l,y:t,width:w-l-right,height:h-t-b}));svg.append(clip);
  const group=svgNode('g',{'clip-path':`url(#${id}-clip)`});svg.append(group);
  series.forEach((s,i)=>{
   let path='',previous=null;
   for(const p of s.points){if(!p.every(Number.isFinite)){previous=null;continue;}const[x,y]=p;
    path+=previous?(s.step?`L${X(x)} ${Y(previous[1])}L${X(x)} ${Y(y)}`:`L${X(x)} ${Y(y)}`):`M${X(x)} ${Y(y)}`;previous=p;
    if(s.dots)group.append(svgNode('circle',{cx:X(x),cy:Y(y),r:3,fill:colors[i%4]}));
   }group.append(svgNode('path',{d:path,fill:'none',stroke:colors[i%4],'stroke-width':1.8,'stroke-dasharray':i?'6 3':''}));
   const span=document.createElement('span');span.textContent=s.name;span.style.borderTop=`3px ${i?'dashed':'solid'} ${colors[i%4]}`;legend.append(span);
  });
 }
 const trace=(rows,col,name,x=0,scale=1000,offset=0,step=false)=>({name,step,points:rows.map(r=>[(r[x]-offset)*scale,r[col]])});
 function inspect(){
  if(!state.tables||state.kind!==0)return;
  const r=state.tables[1][+$('sample').value];if(!r)return;
  $('flow-command').textContent=`Iq* ${fmt(r[1])} A`;
  $('flow-feedback').textContent=`Id ${fmt(r[8])} / Iq ${fmt(r[9])}`;
  $('flow-voltage').textContent=`${fmt(r[10])} / ${fmt(r[11])}`;
  $('flow-ab').textContent=`${fmt(r[12])} / ${fmt(r[13])}`;
  $('flow-duty').textContent=r.slice(17,20).map(x=>(100*x).toFixed(1)+'%').join(' / ');
  $('flow-bridge').textContent=`${fmt(r[20],1)} V · gates ${r[37]?'enabled':'off'}`;
  $('flow-current').textContent=[r[5],r[44],r[45]].map(x=>fmt(x)).join(' / ');
  $('flow-sensor').textContent=`${fmt(r[6])} → ${fmt(r[7])}`;
  for(let k=0;k<6;k++){const on=!!r[25+k];$('gate-'+k).classList.toggle('on',on);$('gate-'+k).textContent=fields[25+k]+' '+(on?'ON':'OFF');}
  ['a','b','c'].forEach((p,k)=>$('pole-'+p).textContent=`${p.toUpperCase()}: ${fmt(r[14+k],1)} V`);
  $('sample-readout').textContent=`t = ${(r[0]*1000).toFixed(6)} ms · interval ${(r[47]*1e6).toFixed(3)} μs · held current age at controller tick ${(r[41]*1e6).toFixed(1)} μs · fault ${r[39]}`;
 }
 function electrical(){
  const [control,wave]=state.tables;
  chart('current-chart',[trace(control,1,'Reference Iq'),trace(control,4,'True Iq'),trace(control,9,'Observed Iq')],'Time [ms]','Current [A]');
  const k={A:0,B:1,C:2}[$('phase').value],base=.118,period=50;
  const blank=wave.find(r=>!r[25+k*2]&&!r[26+k*2]);const center=blank?(blank[0]-base)*1e6:25;
  const range=$('zoom').checked?[center-1,center+1]:[0,period];let s,unit;
  switch($('wave').value){
   case 'gates':s=[trace(wave,25+k*2,'High gate',0,1e6,base,true),trace(wave,26+k*2,'Low gate',0,1e6,base,true),trace(wave,46,'Carrier',0,1e6,base)];unit='Logic / carrier';break;
   case 'voltage':s=[trace(wave,14+k,'Phase–neutral',0,1e6,base,true),{name:'Line voltage',step:true,points:wave.map(r=>[(r[0]-base)*1e6,r[14+k]-r[14+(k+1)%3]])}];unit='Voltage [V]';break;
   case 'sensing':s=[trace(wave,5,'True ia',0,1e6,base),trace(wave,6,'Analog ia',0,1e6,base),trace(wave,7,'Held ADC ia',0,1e6,base,true)];unit='Current [A]';break;
   default:s=[trace(wave,[5,44,45][k],`True i${$('phase').value.toLowerCase()}`,0,1e6,base)];unit='Current [A]';
  }chart('switch-chart',s,'Time from 118 ms [μs]',unit,range);inspect();
 }
 function draw(){
  if(!state.tables)return;
  if(state.kind===0)electrical();
  if(state.kind===1){const r=state.tables[2];chart('heat-chart',[trace(r,1,'Winding',0,1),trace(r,2,'Case',0,1),trace(r,3,'FET proxy',0,1)],'Time [s]','Temperature [°C]');const last=r.at(-1);$('heat-result').textContent=`At 600 s: winding ${fmt(last[1],2)} °C, case ${fmt(last[2],2)} °C, per-phase resistance ${fmt(last[4]*1000,2)} mΩ. Prescribed current; not a protected drive.`;}
  if(state.kind===2){const r=state.tables[3];const currents=[2,5,10,15];
   chart('torque-chart',currents.map(a=>({name:`Iq* ${a} A`,dots:true,points:r.filter(x=>x[1]===a).map(x=>[x[0],x[2]])})),'Motor speed [RPM]','Shaft torque [N·m]');
   chart('efficiency-chart',currents.map(a=>({name:`Iq* ${a} A`,dots:true,points:r.filter(x=>x[1]===a).map(x=>[x[0],x[6]?100*x[5]:NaN])})),'Motor speed [RPM]','Qualified η [%]');
   $('map-result').textContent=`${r.filter(x=>x[6]).length} / ${r.length} points qualify. Motor shaft only; cold-start synthetic map, not continuous rating.`;
   $('map-rows').innerHTML=r.map(x=>`<tr><td>${x[0]}</td><td>${x[1]}</td><td>${fmt(x[2])}</td><td>${fmt(x[3],2)}</td><td>${x[6]?fmt(100*x[5],1):'—'}</td><td>${fmt(x[9]*100,1)}</td><td>${x[10]}</td></tr>`).join('');
  }
  const r=state.tables[3]?.[0];$('exp-summary').textContent=state.kind===0&&r?`Last 20 ms: DC ${fmt(r[3],2)} W · shaft ${fmt(r[4],2)} W · Iq error ${fmt(r[8])} A · saturation ${fmt(100*r[9],1)}% · fault ${r[10]} · numerical power residual ${r[15].toExponential(2)} W`:'';
 }
 $('sample').oninput=inspect;['wave','phase','zoom'].forEach(id=>$(id).onchange=draw);
 $('replay').onclick=()=>{if(state.replay){stopReplay();return;}if(!state.tables)return;$('replay').textContent='Pause sample replay';state.replay=setInterval(()=>{$('sample').value=(+$('sample').value+1)%state.tables[1].length;inspect();},50);};
 $('export-exp').onclick=()=>{
  if(!state.tables)return;const table=state.kind===0?1:state.kind===1?2:3;
  const names=table===1?fields:table===2?['time_s','winding_C','case_C','fet_C','phase_resistance_ohm','copper_W']:mapFields;
  const csv=names.join(',')+'\n'+state.tables[table].map(r=>r.slice(0,names.length).join(',')).join('\n');
  const url=URL.createObjectURL(new Blob([csv],{type:'text/csv'})),a=document.createElement('a');a.href=url;a.download=`actuator-powertrain-${state.kind}.csv`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
 };
 window.addEventListener('resize',()=>{clearTimeout(state.resize);state.resize=setTimeout(draw,150);});
 document.addEventListener('visibilitychange',()=>{if(document.hidden)stopReplay();});
})();
