import * as THREE from './vendor/three.module.js';
import {OrbitControls} from './vendor/OrbitControls.js';
const $=id=>document.getElementById(id), ui={ready:false,running:false,scenario:'tracking',rows:[],snapshot:null,modelLoaded:false,errors:[]};
window.labState=ui;
const states=['Disabled','Calibrating','Ready','Armed','Fault'];
const worker=new Worker('worker.js');let chart='position',renderer,camera,orbit,world,rotor,output,closed=false;
const originalMaterials=new Map(),stopParts=[],proxyParts=[];const axes=new THREE.Vector3(0,1,0);
function report(message){ui.errors.push(message);$('error').hidden=false;$('error').textContent=message+' Reload the page to retry. For file:// use the hosted site or a local HTTP server.';$('engine-status').textContent='Engine unavailable';$('engine-status').className='status fault';}
worker.onerror=e=>report(e.message||'Worker failed to load');
function send(d){if(ui.ready)worker.postMessage(d);}
function values(){return {mode:+$('mode').value,position:+$('target').value,kp:+$('kp').value,kd:+$('kd').value,load:+$('load').value,velocity:+$('velocity').value,torque:+$('torque').value,current:+$('current').value};}
function setRunning(v){if(!ui.ready)return;worker.postMessage({type:'run',value:v});ui.running=v;$('run').textContent=v?'Pause simulation':'Start simulation';$('step').disabled=v;}
function reset(start=false){
 if(!ui.ready)return;ui.rows=[];ui.snapshot=null;$('notice').hidden=true;setRunning(false);
 worker.postMessage({type:'reset',scenario:ui.scenario,algorithm:+$('algorithm').value,values:values()});
 worker.postMessage({type:'set',key:5,value:$('contact').checked?1:0});
 if(start)setRunning(true);
}
function choose(name){
 ui.scenario=name;$('mode').value=name==='reverse'?'2':'4';$('target').value=name==='contact'?'.85':'.4';$('load').value='0';$('kp').value='30';$('kd').value='1.8';for(const id of ['velocity','torque','current'])$(id).value='0';$('contact').checked=['contact','reverse'].includes(name);if(name==='reverse')$('velocity').value='-3';
 document.querySelectorAll('.preset').forEach(b=>b.classList.toggle('active',b.dataset.case===name));
 refreshInputs();reset(true);$('simulator').scrollIntoView({block:'start',behavior:'smooth'});
}
function refreshInputs(){
 const q=+$('target').value;$('target-val').textContent=`${q.toFixed(2)} rad / ${Math.round(q*180/Math.PI)}°`;$('load-val').textContent=`${(+$('load').value).toFixed(1)} N·m`;
 stopParts.forEach(p=>p.visible=$('contact').checked);
}
for(const [id,key] of [['mode',0],['target',1],['velocity',2],['torque',3],['load',4],['current',8],['kp',9],['kd',10]]){
 $(id).addEventListener(id==='target'||id==='load'?'input':'change',()=>{
  const e=$(id);if(!e.checkValidity()){e.reportValidity();return;}refreshInputs();send({type:'set',key,value:+e.value});
 });
}
$('run').onclick=()=>setRunning(!ui.running);$('reset').onclick=()=>reset();$('step').onclick=()=>send({type:'step'});
$('push').onclick=()=>send({type:'set',key:7,value:4});$('fault').onclick=()=>send({type:'set',key:6,value:1});
$('contact').onchange=()=>{refreshInputs();send({type:'set',key:5,value:$('contact').checked?1:0});};
$('algorithm').onchange=()=>reset();$('speed').onchange=()=>send({type:'rate',value:+$('speed').value});
for(const b of document.querySelectorAll('[data-case]'))b.onclick=()=>choose(b.dataset.case);
for(const b of document.querySelectorAll('[data-lesson]'))b.onclick=()=>choose(b.dataset.lesson);
for(const b of document.querySelectorAll('[data-chart]'))b.onclick=()=>{chart=b.dataset.chart;document.querySelectorAll('[data-chart]').forEach(x=>x.classList.toggle('selected',x===b));drawChart();};
const telemetry=['time_s','output_angle_rad','output_speed_rad_s','id_A','iq_A','iq_reference_A','gear_torque_Nm','bus_V','rotor_angle_rad','winding_C','fet_C','state','fault','gates_on','duty_a','duty_b','duty_c','interval_peak_phase_current_A','target_angle_rad','external_load_Nm','stop_reaction_Nm','torque_reference_Nm','voltage_saturated','break_latched','rotor_speed_rad_s','current_reference_limited','fatal','stop_lower_limit_rad','stop_upper_limit_rad','stop_enabled','stop_penetration_rad'];
$('export').onclick=()=>{
 const csv=telemetry.join(',')+'\n'+ui.rows.map(r=>r.join(',')).join('\n');
 const url=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));const a=document.createElement('a');a.href=url;a.download='actuator-live-1kHz.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
let lastUi=0;
worker.onmessage=({data:d})=>{
 if(d.type==='rejected'){$('contact').checked=false;refreshInputs();$('notice').textContent=d.message;$('notice').hidden=false;return;}
 if(d.type==='error'){report(d.message);setRunning(false);return;}
 if(d.type==='ready'){
  ui.ready=true;for(const id of ['run','reset','step','push','fault','export'])$(id).disabled=false;
  $('engine-status').textContent='Ready · C++ WebAssembly';$('run-note').textContent='Start a virtual experiment';reset();const query=new URLSearchParams(location.search),preset=query.get('experiment');if(['tracking','disturbance','contact','reverse','fault'].includes(preset))choose(preset);else if(query.get('paused')!=='1'&&!document.hidden)setRunning(true);return;
 }
 if(d.type==='running'){ui.running=d.value;if(ui.snapshot)update(ui.snapshot);return;}
 if(d.type==='reset')ui.rows=[];
 const rows=d.rows||(d.row?[d.row]:[]);
 if(rows.length){ui.rows.push(...rows);if(ui.rows.length>8000)ui.rows.splice(0,ui.rows.length-8000);ui.snapshot=rows.at(-1);if(performance.now()-lastUi>25||d.type==='reset'){update(ui.snapshot);lastUi=performance.now();}}
};
function update(r){
 if(r.length>=31){$('contact').checked=!!r[29];refreshInputs();$('stop-limits').textContent=r[29]?`Free arc ${r[27].toFixed(3)} to ${r[28].toFixed(3)} rad · deflection ${(r[30]*1000).toFixed(2)} mrad`:'Stop disabled · no obstacle contact';}
 const n=(id,x,d=2,unit='')=>$(id).innerHTML=`${x.toFixed(d)} <small>${unit}</small>`;
 $('time').textContent=r[0].toFixed(3)+' s';n('q',r[1],3,'rad');n('tau',r[6],2,'N·m');n('iq',r[4],2,'A');n('bus',r[7],1,'V');
 $('angle-error').textContent=`error ${(r[18]-r[1]>=0?'+':'')+(r[18]-r[1]).toFixed(3)} rad`;
 $('contact-force').textContent=`stop reaction ${r[20].toFixed(2)} N·m`;$('current-ref').textContent=`target ${r[5].toFixed(2)} A`;
 $('gate').textContent=`gates ${r[13]?'ON':'OFF'} · ${states[r[11]]||'unknown'}`;$('drive-state').textContent=states[r[11]]||'Unknown';
 $('temperature').textContent=`${r[9].toFixed(1)} / ${r[10].toFixed(1)} °C`;$('saturation').textContent=r[22]?'Limited by bus voltage':'No';
 for(let i=0;i<3;i++){const e=$('duty-'+i);e.style.width=(r[14+i]*100)+'%';$('duty-value-'+i).textContent=(r[14+i]*100).toFixed(0)+'%';}
 $('engine-status').textContent=r[12]?`Fault latched · code ${r[12]}`:ui.running?'Running · C++ WebAssembly':'Paused · C++ WebAssembly';
 $('engine-status').className='status'+(r[12]?' fault':ui.running?' running':'');
 let title='Following the target',text='The angle error sets output torque. FOC regulates q-axis current; the gearbox turns the output link.';
 if(r[12]){title='Gates off. Physics continues.';text='The driver fault is latched. Current decays and the load may still move. Reset starts a NEW virtual experiment; it does not acknowledge a real drive.';}
 else if(Math.abs(r[20])>.1){title=r[20]<0?'Reverse contact: opposite face':'Contact: front face';text='The finite stop acts in both directions and on every turn. Signed reaction is a load subtracted from output torque. Small deflection is compliant contact, not free passage.';}
 else if(Math.abs(r[19])>.05){title='A load is pushing on the output';text=`Applied load: ${r[19].toFixed(1)} N·m. In impedance mode, static error is approximately load / Kp. Try Position mode to compare.`;}
 else if(ui.scenario==='disturbance'&&r[0]<.7){title='A disturbance is coming at 0.7 s';text='Watch angle, current, and torque together. A 4 N·m load pulse will last 120 simulated milliseconds. Use Push to repeat it.';}
 else if(ui.scenario==='fault'&&r[0]<.7){title='Driver trip scheduled at 0.7 s';text='A fault will disable the gates. Watch what happens to current and the moving output after electrical drive is removed.';}
 if(!ui.running&&r[0]<.001){title='Start here';text='Press Start, then move the target slider. Add a load to see an impedance controller yield. Nothing is connected to physical hardware.';}
 $('explanation').innerHTML=`<strong>${title}</strong><p>${text}</p>`;
 if(rotor)rotor.quaternion.copy(rotor.userData.base).multiply(new THREE.Quaternion().setFromAxisAngle(axes,r[8]));
 if(output)output.quaternion.copy(output.userData.base).multiply(new THREE.Quaternion().setFromAxisAngle(axes,r[1]));
 drawChart();
}
$('duties').innerHTML=['A','B','C'].map((x,i)=>`<div class="duty"><span>${x}</span><div class="duty-track"><div class="duty-bar" id="duty-${i}"></div></div><output id="duty-value-${i}">50%</output></div>`).join('');
function drawChart(){
 const c=$('plot'),rect=c.getBoundingClientRect(),dpr=Math.min(devicePixelRatio,2),w=rect.width,h=rect.height;
 if(c.width!==Math.round(w*dpr)||c.height!==Math.round(h*dpr)){c.width=Math.round(w*dpr);c.height=Math.round(h*dpr);}
 const ctx=c.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
 const ix=chart==='position'?[1,18,'rad']:chart==='current'?[4,5,'A']:[6,21,'N·m'];
 $('chart-unit').textContent=`${ix[2]} · latest 4 simulated seconds`;
 let r=ui.rows;const end=r.length?r.at(-1)[0]:4,start=Math.max(0,end-4);r=r.filter(x=>x[0]>=start);
 let lo=0,hi=chart==='current'?1:.5;for(const x of r){lo=Math.min(lo,x[ix[0]],x[ix[1]]);hi=Math.max(hi,x[ix[0]],x[ix[1]]);}
 const pad=Math.max(.1,(hi-lo)*.12);lo-=pad;hi+=pad;const left=45,right=w-12,top=7,bottom=h-24;
 const x=t=>left+(t-start)/(Math.max(4,end-start))*(right-left),y=v=>bottom-(v-lo)/(hi-lo)*(bottom-top);
 ctx.font='10px ui-monospace,monospace';ctx.textAlign='right';
 for(let i=0;i<5;i++){const v=lo+(hi-lo)*i/4,Y=y(v);ctx.strokeStyle='#e1e7de';ctx.beginPath();ctx.moveTo(left,Y);ctx.lineTo(right,Y);ctx.stroke();ctx.fillStyle='#75836f';ctx.fillText(v.toFixed(2),left-7,Y+3);}
 ctx.textAlign='center';for(let i=0;i<=4;i++){const t=start+i;ctx.fillStyle='#75836f';ctx.fillText(t.toFixed(1)+' s',x(t),h-5);}
 for(let s=1;s>=0;s--){ctx.beginPath();ctx.strokeStyle=s?'#b17b32':'#21745d';ctx.lineWidth=s?1.4:1.9;ctx.setLineDash(s?[5,3]:[]);const stride=Math.max(1,Math.floor(r.length/(w*2)));let first=true;for(let i=0;i<r.length;i+=stride){const px=x(r[i][0]),py=y(r[i][ix[s]]);if(first){ctx.moveTo(px,py);first=false;}else ctx.lineTo(px,py);}ctx.stroke();}ctx.setLineDash([]);
}
const nums=(s,def=[])=>s?s.split(/\s+/).map(Number):def;
async function setupScene(){
 const response=await fetch('scene.json');if(!response.ok)throw Error('Model asset is missing');const data=await response.json();
 renderer=new THREE.WebGLRenderer({canvas:$('scene'),antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.2;
 world=new THREE.Scene();world.background=new THREE.Color('#1c272b');
 camera=new THREE.PerspectiveCamera(38,1,.005,30);camera.up.set(0,0,1);
 orbit=new OrbitControls(camera,renderer.domElement);orbit.enableDamping=true;orbit.minDistance=.1;orbit.maxDistance=3;orbit.target.set(.05,0,.43);
 world.add(new THREE.HemisphereLight(0xe5f1ed,0x374044,2.2));
 const light=new THREE.DirectionalLight(0xffffff,3.2);light.position.set(1,-1,2);light.castShadow=true;light.shadow.mapSize.set(2048,2048);light.shadow.camera.left=-1;light.shadow.camera.right=1;light.shadow.camera.top=1;light.shadow.camera.bottom=-1;light.shadow.bias=-.00015;world.add(light);
 const fill=new THREE.DirectionalLight(0xadcad7,1.4);fill.position.set(-1,.4,1);world.add(fill);
 const materials={},meshes={};
 for(const [name,a] of Object.entries(data.materials)){const rgba=nums(a.rgba,[.48,.55,.59,1]);materials[name]=new THREE.MeshStandardMaterial({color:new THREE.Color().setRGB(...rgba.slice(0,3),THREE.LinearSRGBColorSpace),metalness:/silver|steel|metal|shell|copper/.test(name)?.65:.15,roughness:/silver|steel|metal/.test(name)?.30:.55,side:THREE.DoubleSide});}
 for(const [name,m] of Object.entries(data.meshes)){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(m.vertices,3));g.setIndex(m.indices);g.computeVertexNormals();meshes[name]=g;}
 function object(n,parent){
  const a=n.a;let obj;
  if(n.tag==='geom'){
   const proxy=a.group==='3';
   if(a.name==='floor'||(proxy&&!['link','tip'].includes(a.name))||(!proxy&&nums(a.rgba,[0,0,0,1])[3]===0))return;
   const s=nums(a.size,[.01,.01,.01]),type=a.type||'sphere';let g;
   if(a.fromto){const f=nums(a.fromto),A=new THREE.Vector3(...f.slice(0,3)),B=new THREE.Vector3(...f.slice(3));const d=A.distanceTo(B);g=type==='capsule'?new THREE.CapsuleGeometry(s[0],d,5,20):new THREE.CylinderGeometry(s[0],s[0],d,28);obj=new THREE.Mesh(g,materials[a.material]);obj.position.copy(A).add(B).multiplyScalar(.5);obj.quaternion.setFromUnitVectors(new THREE.Vector3(0,1,0),B.sub(A).normalize());}
   else{
    if(type==='mesh')g=meshes[a.mesh];else if(type==='box')g=new THREE.BoxGeometry(2*s[0],2*s[1],2*s[2]);else if(type==='cylinder'){g=new THREE.CylinderGeometry(s[0],s[0],2*s[1],48);g.rotateX(Math.PI/2);}else g=new THREE.SphereGeometry(s[0],28,18);
    const rgba=nums(a.rgba);let mat=materials[a.material];if(rgba.length)mat=new THREE.MeshStandardMaterial({color:new THREE.Color().setRGB(...rgba.slice(0,3),THREE.LinearSRGBColorSpace),roughness:.4});
    obj=new THREE.Mesh(g,mat||materials.metal);obj.position.fromArray(nums(a.pos,[0,0,0]));const q=nums(a.quat,[1,0,0,0]);obj.quaternion.set(q[1],q[2],q[3],q[0]);
   }
   if(proxy){obj.material=new THREE.MeshBasicMaterial({color:0xf9d47b,wireframe:true,transparent:true,opacity:.55});obj.visible=false;proxyParts.push(obj);}
   obj.castShadow=!proxy;obj.receiveShadow=!proxy;
  }else{obj=new THREE.Group();obj.position.fromArray(nums(a.pos,[0,0,0]));const q=nums(a.quat,[1,0,0,0]);obj.quaternion.set(q[1],q[2],q[3],q[0]);}
  obj.name=a.name||n.tag;parent.add(obj);obj.userData.base=obj.quaternion.clone();
  if(obj.name==='rotor')rotor=obj;if(obj.name==='output')output=obj;
  if(obj.name==='impact_ball')obj.visible=false;
  if(obj.name==='stopper'||obj.name.startsWith('stop_'))stopParts.push(obj);
  if(['motor_housing','gear_housing','front_face','rear_cap'].includes(obj.name))originalMaterials.set(obj,true);
  for(const child of n.children)if(child.tag!=='joint')object(child,obj);
 }
 object(data.world,world);
 const grid=new THREE.GridHelper(4,40,0x40545a,0x2f4045);grid.rotation.x=Math.PI/2;grid.position.z=-.003;world.add(grid);
 function resize(){const r=$('viewport').getBoundingClientRect();renderer.setSize(r.width,r.height,false);camera.aspect=r.width/r.height;camera.updateProjectionMatrix();drawChart();}
 new ResizeObserver(resize).observe($('viewport'));resize();
 function focus(which){
  const points={bench:[[.84,-1.30,1.04],[.05,0,.43]],motor:[[.17,-.33,.76],[0,.005,.60]],board:[[.43,-.18,.43],[.29,.025,.20]]};
  camera.position.fromArray(points[which][0]);orbit.target.fromArray(points[which][1]);orbit.update();
  for(const name of ['bench','motor','board'])$('view-'+name).classList.toggle('selected',which===name);
 }
 for(const v of ['bench','motor','board'])$('view-'+v).onclick=()=>focus(v);
 $('cutaway').onclick=()=>{closed=!closed;for(const o of originalMaterials.keys())o.visible=!closed;$('cutaway').setAttribute('aria-pressed',String(closed));$('cutaway').textContent=closed?'Close motor':'Open motor';$('scene-label').textContent=closed?'Fixed windings · spinning rotor / lumped gearbox':'QDD / 6:1 compliant drive';};
 $('colliders').onclick=()=>{const show=$('colliders').getAttribute('aria-pressed')!=='true';$('colliders').setAttribute('aria-pressed',String(show));proxyParts.forEach(o=>o.visible=show);};
 const ray=new THREE.Raycaster(),mouse=new THREE.Vector2();let down;
 renderer.domElement.addEventListener('pointerdown',e=>down=[e.clientX,e.clientY]);
 renderer.domElement.addEventListener('pointerup',e=>{if(!down||Math.hypot(e.clientX-down[0],e.clientY-down[1])>4)return;const rect=renderer.domElement.getBoundingClientRect();mouse.set((e.clientX-rect.left)/rect.width*2-1,-(e.clientY-rect.top)/rect.height*2+1);ray.setFromCamera(mouse,camera);const hit=ray.intersectObjects(world.children,true).find(h=>h.object.isMesh&&h.object.visible);if(hit)$('picked').textContent=hit.object.name.replaceAll('_',' ');});
 focus('bench');refreshInputs();ui.modelLoaded=true;
 function frame(){requestAnimationFrame(frame);orbit.update();renderer.render(world,camera);}frame();
}
setupScene().catch(e=>report('3D viewer: '+e.message));
const filmData=[['tracking','Position tracking','A target change drives current, torque, and link motion.'],['disturbance','External disturbance','The controller reacts to an applied load.'],['contact','Stopper contact','The moving tip pushes against a physical stop.'],['impact','Falling-object impact','A free body hits the driven link.'],['fault','Driver fault','Drive is removed while mechanical dynamics continue.']];
$('films').innerHTML=filmData.map(([id,title,text])=>`<article class="film" id="film-${id}"><video controls muted loop playsinline preload="metadata" poster="media/${id}.jpg" aria-label="Actual MuJoCo: ${title}"><source src="media/${id}.webm" type='video/webm; codecs="vp9"'><source src="media/${id}.mp4" type="video/mp4"><a href="media/${id}.mp4">Play ${title}</a></video><div class="film-info"><h3>${title}</h3><p>${text}</p><p class="video-status" role="status">Event excerpt · loops from the start; press Play</p><a href="media/${id}.mp4">Event MP4 ↗</a> · <a href="media/full/${id}.mp4">Full recording ↗</a> · <a href="media/${id}.webm">Open WebM ↗</a></div></article>`).join('');
// A rejected nested <source> need not reject play(). Expose source errors too.
for (const film of document.querySelectorAll('.film')) {
 const video=film.querySelector('video'),status=film.querySelector('.video-status');
 const sources=[...video.querySelectorAll('source')],failed=new Set();
 const failedPlayback=()=>{status.textContent='This browser could not play the recording. Try the MP4 or WebM link below.';};
 for(const source of sources)source.addEventListener('error',()=>{failed.add(source);if(failed.size===sources.length)failedPlayback();});
 video.addEventListener('error',failedPlayback);
 video.addEventListener('playing',()=>{status.textContent='Looping actual MuJoCo event · restart is a replay, not physical reversal';});
 video.addEventListener('pause',()=>{if(!video.error)status.textContent='Recording paused · press Play to continue';});
}
fetch('build.json').then(r=>r.json()).then(b=>{$('version').textContent=`Source ${b.source_commit.slice(0,12)} · local C++/WASM · 1 kHz telemetry`;$('version').dataset.sha=b.source_commit;}).catch(()=>{$('version').textContent='Build metadata unavailable';});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&ui.running)setRunning(false);});
document.addEventListener('keydown',e=>{if(e.code==='Space'&&!['INPUT','SELECT','BUTTON','TEXTAREA'].includes(document.activeElement.tagName)){e.preventDefault();setRunning(!ui.running);}});
// Zero feed-forward defaults are also visible in the form before the engine starts.
for(const id of ['velocity','torque','current'])$(id).value='0';
refreshInputs();drawChart();
