/* Read-only presentation of one live C++ instance. No numerical plant/FOC here. */
export function createLivePipeline(selectChart){
 const $=id=>document.getElementById(id),keys=['command','foc','bridge','motion','sensor'];
 const buttons=Array.from(document.querySelectorAll('[data-live-stage]'));
 const f=(v,n=2)=>Number(v).toFixed(n),modes=['Current','Torque','Velocity','Position','Impedance'];
 let selected='command',latest=null;
 const content={
  command:['01 / Set the command','The active mode converts its command and feedback into a torque or current request. Only the command panel changes this joint.','control.html#pipeline'],
  foc:['02 / Close the current loop','Clarke / Park transforms offset-corrected, held ADC samples into dq. The current controller regulates Id / Iq and produces voltage-limited Vd / Vq at 20 kHz.','control.html#foc'],
  bridge:['03 / Apply duty through the inverter','SVPWM converts the limited voltage vector into three duty ratios. This live bridge is averaged: no individual gate-edge or ripple trace is resolved here.','electronics.html'],
  motion:['04 / Drive the output','Motor torque acts through a compliant 6:1 gearbox. Load, friction and a finite stop oppose motion. Output angle and gearbox torque below are plant states.','control.html#contact'],
  sensor:['05 / Measure, then feed back to 02','Physical phase current is analog-filtered, then capture noise and ADC quantization are added. The held sample is delayed; FOC corrects its offset and aligns its angle.','control.html#timing']
 };
 function choose(key,focus=false){
  if(!keys.includes(key))return;selected=key;
  buttons.forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.liveStage===key)));
  const [title,description,href]=content[key];$('pipeline-title').textContent=title;$('pipeline-description').textContent=description;
  $('pipeline-link').href=href;$('pipeline-link').textContent=key==='bridge'?'Separate switching experiment ↗':'Read the explanation ↗';
  $('pipeline-next').textContent=key==='sensor'?'Back to command ↺':'Next stage →';
  if(latest){draw(...latest);const mode=latest[1][18];selectChart(key==='foc'||key==='sensor'||key==='bridge'?'current':mode===0?'current':mode===1?'torque':mode===2?'speed':'position');}
  if(focus)buttons[keys.indexOf(key)].focus();
 }
 buttons.forEach((b,i)=>{b.onclick=()=>choose(b.dataset.liveStage);b.onkeydown=e=>{if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();choose(keys[(i+(e.key==='ArrowRight'?1:4))%5],true);}};});
 $('pipeline-next').onclick=()=>choose(keys[(keys.indexOf(selected)+1)%5]);
 function draw(r,s,requested){
  if(!s||s.length!==28||r[0]!==s[0]){$('pipeline-time').textContent='Waiting for a matching controller snapshot';$('live-pipeline').dataset.synchronized='false';return;}
  latest=[r,s,requested];$('live-pipeline').dataset.synchronized='true';$('live-pipeline').dataset.time=String(r[0]);
  const hasTick=s[1]>=0,active=r[11]===3,mode=s[18];
  const target=mode===0?[s[22],'A']:mode===1?[s[21],'N·m']:mode===2?[s[20],'rad/s']:[s[19],'rad'];
  $('pipeline-time').textContent=hasTick?`Joint ${f(r[0],3)} s · last control tick ${f(s[1]*1000,2)} ms`:'No control tick yet';
  $('pipe-command').textContent=hasTick?`${f(target[0])} ${target[1]}`:'Await first tick';$('pipe-mode').textContent=modes[mode]+' command';
  $('pipe-foc').textContent=active?`${f(s[3])} A`:'Loop inactive';
  $('pipe-bridge').textContent=`${f(r[7],0)} V ${r[13]?'ON':'OFF'}`;$('pipe-gates').textContent=r[13]?'3 duty ratios · averaged':'Gates inhibited · stored energy remains';
  $('pipe-motion').textContent=`${f(r[1],3)} rad`;$('pipe-torque').textContent=`${f(r[6])} N·m · ${f(r[2]*60/(2*Math.PI),1)} RPM`;
  $('pipe-sensor').textContent=hasTick?`${f(s[8])} A`:'Await sample';$('pipe-age').textContent=`Age at control tick ${f(s[6]*1e6,0)} μs · ↺ 02`;
  const expected=[requested.mode,requested.position,requested.velocity,requested.torque,requested.current,requested.kp,requested.kd];
  // Applied-command fields are captured with Drive::tick, never inferred from the editor.
  const pending=hasTick&&expected.some((v,k)=>Math.abs(v-s[18+k])>1e-5);
  $('pipeline-pending').hidden=!pending;
  const details={
   command:`${modes[mode]} · torque request ${f(r[21])} N·m → Iq* ${f(r[5])} A${mode===0?' (direct current command)': ''}`,
   foc:active?`${s[25]?'Predictive':'PI FOC'} · Id* / Iq* ${f(s[26])} / ${f(r[5])} A · observed ${f(s[2])} / ${f(s[3])} A · Vd / Vq ${f(s[4])} / ${f(s[5])} V`:'Current regulation inactive: the displayed voltage command is not an energized bridge.',
   bridge:`Duty A / B / C ${r.slice(14,17).map(v=>f(v*100,1)+'%').join(' / ')} · gates ${r[13]?'ON':'OFF'} · ${r[22]?'voltage limited':'not voltage limited'}`,
   motion:`Output ${f(r[1],3)} rad · speed ${f(r[2],2)} rad/s · gear torque ${f(r[6])} N·m · load / stop ${f(r[19])} / ${f(r[20])} N·m`,
   sensor:`At joint endpoint: true ia ${f(s[11],3)} A → analog ia ${f(s[17],3)} A. Held raw ADC ia ${f(s[8],3)} A · sampled bus ${f(s[27],1)} V (earlier acquisition).`
  };
  $('pipeline-values').textContent=hasTick?details[selected]:'No computed controller sample yet. Start or advance the joint.';
 }
 choose('command');
 return {update:draw};
}
