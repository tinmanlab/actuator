import {decodeBridge,selectSample,edgeTimes,advanceCursor} from './pwm-state.js';
/* One explicit presentation clock; existing native gates/held currents are immutable.
 * The instantaneous vector is IDEAL and hidden for dead-time/off. No Vgs/ripple claim. */
export function createPwmInspector(plot){
 const $=id=>document.getElementById(id),ns='http://www.w3.org/2000/svg',colors=['#5ce4bb','#ffc971','#6aaeff'];
 const reduce=matchMedia('(prefers-reduced-motion: reduce)');
 const state={latest:null,capture:null,phase:12.5e-6,playing:false,liveRunning:false,manual:false,last:null,cycles:0,frames:0};
 window.pwmInspectorState=state;
 const node=(tag,a={},text)=>{const n=document.createElementNS(ns,tag);for(const[k,v]of Object.entries(a))n.setAttribute(k,v);if(text!==undefined)n.textContent=text;return n;};
 const fmt=(x,n=2)=>Number.isFinite(x)?x.toFixed(n):'—';
 const circuit=$('bridge-circuit'),vector=$('vector-diagram');
 for(let k=0;k<6;k++){
  const angle=k*Math.PI/3,x=170+94*Math.cos(angle),y=110-94*Math.sin(angle);
  vector.append(node('line',{x1:170,y1:110,x2:x,y2:y,stroke:'#385566'}),node('text',{x:170+115*Math.cos(angle),y:114-108*Math.sin(angle),'text-anchor':'middle',class:'vector-label'},['100','110','010','011','001','101'][k]));
 }
 vector.append(node('path',{id:'duty-vector',fill:'none',stroke:'#5ce4bb','stroke-width':3,'marker-end':'url(#mean-tip)'}),
  node('path',{id:'switch-vector',fill:'none',stroke:'#ffc971','stroke-width':3,'marker-end':'url(#switch-tip)'}),
  node('circle',{id:'zero-vector',cx:170,cy:110,r:7,fill:'#ffc971'}));
 const tokens=[],paths=[];
 for(let p=0;p<3;p++){
  const x=57+p*111,color=colors[p];
  circuit.append(node('path',{d:`M${x} 28V220 M${x} 124h48`,stroke:'#526a7b',fill:'none','stroke-width':2}));
  const path=node('path',{id:'current-path-'+p,fill:'none',stroke:color,'stroke-width':3});paths.push(path);circuit.append(path);
  for(let side=0;side<2;side++){
   const y=side?165:57,k=p*2+side;
   circuit.append(node('rect',{id:'circuit-gate-'+k,x:x-20,y,width:40,height:32,rx:4,class:'gate'}),
    node('text',{x,y:y+21,'text-anchor':'middle',fill:'#dfecf4'},'ABC'[p]+(side?'L':'H')));
  }
  circuit.append(node('circle',{cx:x,cy:124,r:3,fill:color}),node('text',{x:x+49,y:111,'text-anchor':'end',fill:color},'UVW'[p]),
   node('path',{d:`M${x+38} 117l10 7-10 7`,stroke:color,fill:'none','stroke-width':2}));
  const token=node('circle',{id:'flow-token-'+p,r:4,fill:color,visibility:'hidden'});tokens.push(token);circuit.append(token);
 }
 function controls(){
  $('pwm-play').textContent=state.playing?'Pause cycle':'Play PWM cycle';$('pwm-play').setAttribute('aria-pressed',String(state.playing));
  $('pwm-mode-status').textContent=state.playing?'Slow playback · 3 s per 50 μs cycle':'Paused inspection · drag or step';
  $('bridge-instrument').dataset.playing=String(state.playing);
 }
 function stop(){state.playing=false;state.last=null;controls();render();}
 function capture(){
  if(!state.latest)return false;state.capture=state.latest;
  state.phase=0;state.last=null;wavePlot();render();return true;
 }
 function wavePlot(){
  if(!state.capture)return;const rows=state.capture.rows,mode=$('pwm-interface').value==='3';
  const cols=mode?[1,2,3]:[4,5,10];
  plot('gate-scope',cols.map(c=>({step:c!==10,points:rows.map(a=>[a[0]*1e6,a[c]])})),'Logic 0/1'+(mode?' · A/B/C':' · AH/AL/carrier'),{range:[0,50],yrange:[-.1,1.1],xunit:'μs'});
  $('pwm-owner').textContent=mode?'3PWM requests A/B/C → driver adds complementary gates and 150 ns dead time.':'6PWM: timer supplies AH/AL, BH/BL, CH/CL with 150 ns dead time.';
  $('pwm-owner').textContent+=' Interface view only; the same three-phase bridge is inspected.';
 }
 function render(){
  const c=state.capture;if(!c)return;const sample=selectSample(c.rows,state.phase),s=decodeBridge(sample,c.currents,c.bus,c.enabled);
  state.sample=sample;state.decoded=s;state.frames++;
  const h=sample.slice(4,10);for(let k=0;k<6;k++)$('circuit-gate-'+k).classList.toggle('on',!!h[k]);
  const axis=94/(2*c.bus/3),d=c.duty,mean=(d[0]+d[1]+d[2])/3;
  const alpha=c.enabled?(d[0]-mean)*c.bus:0,beta=c.enabled?(d[1]-d[2])*c.bus/Math.sqrt(3):0;
  $('duty-vector').setAttribute('d',`M170 110L${170+alpha*axis} ${110-beta*axis}`);
  const ideal=s.alpha!==null;
  $('switch-vector').setAttribute('d',ideal?`M170 110L${170+s.alpha*axis} ${110-s.beta*axis}`:'M170 110');
  $('switch-vector').style.visibility=s.kind==='active'?'visible':'hidden';
  $('zero-vector').style.visibility=s.kind==='zero'?'visible':'hidden';
  $('vector-state').textContent=s.kind==='zero'?`${s.label} · ZERO vector`:s.kind==='active'?`${s.label} · active vector`:s.label+' · ideal vector not shown';
  $('vector-mean').textContent=`Mean voltage ${fmt(Math.hypot(alpha,beta))} V · ${fmt(Math.atan2(beta,alpha)*180/Math.PI,1)}°`;
  $('pwm-phase').value=fmt(state.phase*1e6,6);$('pwm-time').textContent=fmt(state.phase*1e6,3)+' μs';
  $('pwm-source').textContent=`Duty/current held from joint ${fmt(c.time,3)} s · ${fmt(c.bus,1)} V. Next automatic capture at cycle restart.`;
  const canvas=$('gate-scope'),width=canvas.getBoundingClientRect().width;
  $('gate-cursor').style.left=(38+state.phase/50e-6*(width-50))+'px';
  for(let p=0;p<3;p++){
   const type=s.paths[p],x=57+p*111,high=type.startsWith('high'),diode=type.includes('diode');
   const start=high?28:220;
   paths[p].setAttribute('d',diode?`M${x} ${start}H${x+26}V124H${x+48}`:`M${x} ${start}V124H${x+48}`);
   paths[p].style.visibility=type==='none'?'hidden':'visible';paths[p].setAttribute('stroke-dasharray',diode?'3 4':'none');
   const direction=c.currents[p]>=0?1:-1;
   const moving=state.playing&&!document.hidden&&type!=='none';tokens[p].style.visibility=moving?'visible':'hidden';
   if(moving){const f=(state.phase/50e-6*4)%1;const at=paths[p].getPointAtLength((direction>0?f:1-f)*paths[p].getTotalLength());tokens[p].setAttribute('cx',at.x);tokens[p].setAttribute('cy',at.y);}
   const label=type==='none'?'negligible current':type==='high'?'high switch':type==='low'?'low switch':high?'high diode':'low diode';
   $('phase-path-'+p).textContent=`${'UVW'[p]} ${fmt(c.currents[p])} A · ${label}`;
  }
  circuit.dataset.time=c.time;circuit.dataset.reconstructed='true';circuit.dataset.kind=s.kind;circuit.dataset.vector=s.label;
 }
 function setRunning(v){state.liveRunning=v;state.manual=false;if(!v)stop();else if(!reduce.matches&&state.capture?.enabled){state.playing=true;controls();}}
 function tick(now){
  const liveRunning=Boolean(window.labState?.running);if(liveRunning!==state.liveRunning)setRunning(liveRunning);
  if(state.playing&&!document.hidden&&state.capture){
   if(state.last!==null){const next=advanceCursor(state.phase,Math.min(now-state.last,100),3000);state.phase=next.phase;if(next.wrapped){state.cycles++;if($('pwm-follow').checked&&state.liveRunning&&state.latest){const phase=state.phase;capture();state.phase=phase;}}}
   state.last=now;render();
  }else state.last=null;
  requestAnimationFrame(tick);
 }
 $('pwm-play').onclick=()=>{state.manual=true;if(state.playing)stop();else{if(!state.capture&&!capture())return;state.playing=true;state.last=null;controls();}};
 $('pwm-phase').oninput=()=>{state.manual=true;state.playing=false;state.last=null;state.phase=+$('pwm-phase').value*1e-6;controls();render();};
 $('pwm-next').onclick=()=>{state.manual=true;stop();if(!state.capture)return;state.phase=edgeTimes(state.capture.rows).find(t=>t>state.phase+1e-12)??0;render();};
 $('pwm-capture').onclick=()=>{state.manual=true;stop();capture();};
 $('pwm-interface').onchange=()=>{wavePlot();render();};
 // Native dialog reuses the same circuit and controls. No duplicated simulation/IDs.
 const dialog=$('pwm-dialog'),instrument=$('bridge-instrument'),placeholder=$('bridge-placeholder');
 function expand(){placeholder.style.height=instrument.getBoundingClientRect().height+'px';placeholder.hidden=false;dialog.append(instrument);dialog.showModal();wavePlot();render();}
 function collapse(){placeholder.before(instrument);placeholder.hidden=true;wavePlot();render();$('pwm-expand').focus();}
 $('pwm-expand').onclick=expand;$('pwm-close').onclick=()=>dialog.close();dialog.addEventListener('close',collapse);
 const diagramDialog=$('diagram-dialog'),diagram=$('foc-diagram'),diagramMarker=$('diagram-placeholder');
 $('diagram-expand').onclick=()=>{diagramMarker.style.height=diagram.getBoundingClientRect().height+'px';diagramMarker.hidden=false;diagramDialog.append(diagram);diagramDialog.showModal();};
 $('diagram-close').onclick=()=>diagramDialog.close();diagramDialog.addEventListener('close',()=>{diagramMarker.before(diagram);diagramMarker.hidden=true;$('diagram-expand').focus();});
 document.addEventListener('visibilitychange',()=>{state.last=null;});
 reduce.addEventListener('change',()=>{if(reduce.matches)stop();});
 requestAnimationFrame(tick);controls();
 return {
  receive(wave,r,s,type){
   if(!wave||!r||!s||wave.time!==r[0]||s[0]!==r[0])return;
   state.latest={time:wave.time,rows:wave.rows,duty:r.slice(14,17),currents:s.slice(11,14),bus:r[7],enabled:!!r[13]};
   if(type==='reset'){stop();state.capture=null;state.manual=false;state.phase=0;}
   const changedGate=state.capture&&state.capture.enabled!==state.latest.enabled;
   if(!state.capture||changedGate||(!state.playing&&!state.manual)){capture();}
   if(!state.latest.enabled){stop();capture();}
   else if(state.liveRunning&&!state.manual&&!reduce.matches){state.playing=true;controls();}
  },
  refresh(){wavePlot();render();}
 };
}
