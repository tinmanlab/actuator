/* Plot native data only. No alternate JavaScript motor model or synthetic fallback. */
(() => {
 const ns='http://www.w3.org/2000/svg';
 const colors=['#17654e','#ac6b24','#4665a5','#963f65'];
 const dash=['','7 4','2 4','10 3 2 3'];
 let data;
 const el=id=>document.getElementById(id);
 const format=v=>Math.abs(v)>=1000?v.toFixed(0):Math.abs(v)>=10?v.toFixed(1):Math.abs(v)>=1?v.toFixed(2):v.toPrecision(3);
 function node(tag,attrs,text){const n=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))n.setAttribute(k,v);if(text!==undefined)n.textContent=text;return n;}
 const series=(rows,key,label,x='time_s',scale=1,shift=0,step=false)=>({label,step,points:rows.map(r=>[(r[x]-shift)*scale,r[key]])});
 function plot(id,seriesList,xLabel,yLabel,window=null){
  const svg=el(id),w=Math.max(320,svg.clientWidth),h=300,l=65,t=18,b=48,right=16;
  svg.replaceChildren();svg.setAttribute('viewBox',`0 0 ${w} ${h}`);
  const all=seriesList.flatMap(s=>s.points);if(!all.length||all.some(p=>!p.every(Number.isFinite)))throw Error('Invalid native plot '+id);
  let xmin=window?window[0]:Math.min(...all.map(p=>p[0])),xmax=window?window[1]:Math.max(...all.map(p=>p[0]));
  const visible=all.filter(p=>p[0]>=xmin&&p[0]<=xmax);
  let ymin=Math.min(...visible.map(p=>p[1])),ymax=Math.max(...visible.map(p=>p[1]));
  const dy=Math.max((ymax-ymin)*.08,Math.abs(ymax)*.005,.001);ymin-=dy;ymax+=dy;
  const X=x=>l+(x-xmin)/(xmax-xmin)*(w-l-right),Y=y=>h-b-(y-ymin)/(ymax-ymin)*(h-t-b);
  const defs=node('defs',{}),clip=node('clipPath',{id:id+'-clip'});clip.append(node('rect',{x:l,y:t,width:w-l-right,height:h-t-b}));defs.append(clip);svg.append(defs);
  const desc=node('desc',{},`${yLabel} versus ${xLabel}. ${seriesList.map(s=>s.label).join('; ')}. Native simulation data; full-resolution CSV in the downloadable archive.`);svg.append(desc);
  for(let i=0;i<=4;i++){
   const v=ymin+(ymax-ymin)*i/4,y=Y(v);svg.append(node('line',{x1:l,x2:w-right,y1:y,y2:y,stroke:'#dbe2da'}));
   svg.append(node('text',{x:l-8,y:y+4,'text-anchor':'end',class:'axis'},format(v)));
  }
  const ticks=w<500?2:4;
  for(let i=0;i<=ticks;i++){const v=xmin+(xmax-xmin)*i/ticks;svg.append(node('text',{x:X(v),y:h-b+21,'text-anchor':'middle',class:'axis'},format(v)));}
  svg.append(node('text',{x:l,y:12,class:'axis'},yLabel),node('text',{x:(l+w-right)/2,y:h-7,'text-anchor':'middle',class:'axis'},xLabel));
  const g=node('g',{'clip-path':`url(#${id}-clip)`});svg.append(g);
  seriesList.forEach((s,i)=>{
   let d='';s.points.forEach(([x,y],k)=>{
    if(!k)d=`M${X(x)} ${Y(y)}`;
    else if(s.step)d+=`L${X(s.points[k-1][0])} ${Y(y)}L${X(x)} ${Y(y)}`;
    else d+=`L${X(x)} ${Y(y)}`;
   });g.append(node('path',{d,fill:'none',stroke:colors[i%4],'stroke-width':1.9,'stroke-dasharray':dash[i%4]}));
  });
  const legend=el(id+'-legend');legend.replaceChildren();
  seriesList.forEach((s,i)=>{const span=document.createElement('span');span.textContent=s.label;span.style.borderTop=`3px ${i===0?'solid':i===2?'dotted':'dashed'} ${colors[i%4]}`;legend.append(span);});
  const inspect=e=>{
   const rect=svg.getBoundingClientRect(),x=xmin+Math.max(0,Math.min(1,(e.clientX-rect.left-l)/(w-l-right)))*(xmax-xmin);
   el(id+'-readout').textContent=seriesList.map(s=>{const p=s.points.reduce((a,b)=>Math.abs(b[0]-x)<Math.abs(a[0]-x)?b:a);return `${s.label}: ${format(p[1])} at ${format(p[0])}`;}).join(' · ');
  };
  svg.onpointermove=inspect;svg.onpointerdown=inspect;
 }
 function draw(){
  if(!data)return;
  const r=data.traces;
  plot('current-plot',[
   series(r.current,'iq_ref_A','Reference [A]','boundary_time_s',1000),
   series(r.current,'iq_A','True iq [A]','time_s',1000),
   series(r.current,'sensor_iq_A','Sensed iq [A]','boundary_time_s',1000)],'Time [ms]','Current [A]',[9,16]);
  plot('modulation-plot',['a','b','c'].map(p=>series(r.modulation,'duty_'+p,'Duty '+p.toUpperCase(),'electrical_deg')),'Electrical angle [deg]','Duty [0–1]');
  const phase=el('pwm-phase').value,kind=el('pwm-signal').value,wave=r.pwm_edges;
  const both=wave.find(v=>v['high_'+phase]===0&&v['low_'+phase]===0);
  const center=(both.time_s-.02)*1e6;
  let ss,unit;
  if(kind==='gates'){ss=[series(wave,'high_'+phase,'High gate','time_s',1e6,.02,true),series(wave,'low_'+phase,'Low gate','time_s',1e6,.02,true)];unit='Gate [0/1]';}
  else if(kind==='current'){ss=[series(wave,'i'+phase+'_A','Phase '+phase.toUpperCase()+' current','time_s',1e6,.02)];unit='Current [A]';}
  else{const next={a:'b',b:'c',c:'a'}[phase];ss=[series(wave,'v'+phase+'_V','Phase-neutral','time_s',1e6,.02,true),{label:'Line '+phase.toUpperCase()+next.toUpperCase(),step:true,points:wave.map(v=>[(v.time_s-.02)*1e6,v['v'+phase+'_V']-v['v'+next+'_V']])}];unit='Voltage [V]';}
  plot('pwm-plot',ss,'Time from 20 ms [μs]',unit,el('pwm-zoom').checked?[center-2,center+2]:null);
  plot('shutdown-plot',[series(r.gate_off,'iq_A','True iq','time_s',1000)],'Time [ms]','Current [A]');
  plot('thermal-plot',[series(r.thermal,'winding_C','Winding'),series(r.thermal,'case_C','Case'),series(r.thermal,'fet_C','Aggregate FET')],'Time [s]','Temperature [°C]');
  plot('derating-plot',[series(r.derating,'iq_ref_A','Limited reference','boundary_time_s',1000),series(r.derating,'iq_A','True iq','time_s',1000)],'Time [ms]','Current [A]');
  plot('friction-plot',[series(r.velocity,'dc_W','Baseline DC input'),series(r.friction,'dc_W','Higher-drag DC input'),series(r.velocity,'load_W','External load output')],'Time [s]','Power [W]');
  const key=el('regen-signal').value;plot('regen-plot',[series(r.regeneration,key,key==='dc_W'?'Inverter DC power':'DC-link voltage')],'Time [s]',key==='dc_W'?'Power [W]':'Voltage [V]');
 }
 fetch('data/signals.json').then(r=>{if(!r.ok)throw Error('HTTP '+r.status);return r.json();}).then(d=>{
  data=d;draw();
  const th=d.traces.thermal.at(-1),m=d.meta.motoring;
  el('thermal-result').textContent=`In this fixture at 600 s: winding ${th.winding_C.toFixed(2)}°C, case ${th.case_C.toFixed(2)}°C; per-phase resistance ${(th.resistance_ohm*1000).toFixed(2)} mΩ. This is a synthetic thermal response, not a measured rating.`;
  el('efficiency-result').textContent=`Over the native 0.8–1.0 s window: mean inverter input ${m.dc_W.toFixed(4)} W, useful load output ${m.load_W.toFixed(4)} W, modeled motoring ratio ${(100*m.efficiency).toFixed(2)}%. Mean stored-energy rate ${m.stored_energy_rate_W.toFixed(5)} W; energy residual ${m.residual_W.toExponential(2)} W. The full trace is in the archive.`;
  el('data-status').textContent='Native experiment data loaded. These are reproducible traces, not live execution on this page.';
  window.signalEvidence={ready:true,thermal:th,motoring:m};
  ['pwm-phase','pwm-signal','pwm-zoom','regen-signal'].forEach(id=>el(id).addEventListener('change',draw));
  let timer;window.addEventListener('resize',()=>{clearTimeout(timer);timer=setTimeout(draw,100);});
 }).catch(e=>{el('data-status').textContent='Could not load native evidence: '+e.message+'. Use the live lab or retry; no substitute curves are shown.';el('data-status').classList.add('warning');window.signalEvidence={ready:false,error:e.message};});
})();
