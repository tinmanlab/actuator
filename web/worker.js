/* Single-threaded WASM in a Worker: no SharedArrayBuffer or cross-origin headers. */
importScripts('wasm/qdd.js');
let core, running=false, timer=null, scenario='tracking', fired=false, rate=1;
const fields=31;
function row(){return Array.from({length:fields},(_,i)=>core._lab_get(i));}
function signal(){return Array.from({length:28},(_,i)=>core._lab_signal(i));}
function set(k,v){if(!core._lab_set(k,v))throw Error(`Rejected control ${k}: ${v}`);}
function reset(data={}){
 running=false;scenario=data.scenario||'tracking';fired=false;
 if(!core._lab_reset(data.algorithm===1?1:0))throw Error('Cannot initialize the C++ model');
 const v=data.values||{};
 set(0,v.mode??4);set(1,v.position??(scenario==='contact'?.85:.4));set(4,v.load??0);
 set(5,['contact','reverse'].includes(scenario)?1:0);set(9,v.kp??30);set(10,v.kd??1.8);
 set(2,v.velocity??0);set(3,v.torque??0);set(8,v.current??0);
 postMessage({type:'reset',row:row(),signal:signal(),scenario});
}
function advance(){
 if(!running)return;
 const data=[];
 // Telemetry at 1 kHz; internal PWM/FOC still 20 kHz, electrical step <=5 us.
 for(let k=0;k<Math.round(20*rate);k++){
  const t=core._lab_get(0);
  if(!fired&&t>=.7&&(scenario==='disturbance'||scenario==='fault')){
   set(scenario==='fault'?6:7,scenario==='fault'?1:4);fired=true;
  }
  if(core._lab_step(20)!==20)throw Error('C++ simulation stopped: invalid/non-finite state');
  data.push(row());
 }
 postMessage({type:'samples',rows:data,signal:signal()});
}
createQdd().then(m=>{
 core=m;reset();postMessage({type:'ready'});
 timer=setInterval(()=>{try{advance();}catch(e){running=false;postMessage({type:'error',message:e.message});}},20);
}).catch(e=>postMessage({type:'error',message:`WebAssembly load failed: ${e.message}`}));
onmessage=({data:d})=>{
 if(!core){postMessage({type:'error',message:'The C++ engine is not ready yet.'});return;}
 try{
  switch(d.type){
   case 'run':running=!!d.value;postMessage({type:'running',value:running});break;
   case 'reset':reset(d);break;
   case 'set':
    if(d.key===5&&Number(d.value)===1){
     if(!core._lab_set(5,1))postMessage({type:'rejected',key:5,message:'Stop not enabled: the link is inside its solid volume. Move clear or reset first.'});
    }else set(d.key,Number(d.value));
    break; // Commands are not telemetry samples.
   case 'rate':rate=d.value===.25?.25:1;break;
   case 'step':if(!running){if(core._lab_step(20)!==20)throw Error('Step failed');postMessage({type:'samples',rows:[row()],signal:signal()});}break;
  }
 }catch(e){postMessage({type:'error',message:e.message});}
};
