/* Numerical work is isolated from drawing. The WASM API owns all physics. */
importScripts('wasm/experiments.js');
let core;
createExperiments().then(m=>{core=m;postMessage({type:'ready'});}).catch(e=>postMessage({type:'error',message:e.message}));
onmessage=({data:d})=>{
 if(!core||d.type!=='run')return;
 try{
  const p=d.parameters;
  if(!Array.isArray(p)||p.length!==8||!p.every(Number.isFinite)||!core._exp_run(...p))throw Error('Experiment rejected. Check finite inputs and limits.');
  const tables=Array.from({length:4},(_,t)=>{
   const n=core._exp_rows(t),a=new Float64Array(n*48);
   for(let r=0;r<n;r++)for(let c=0;c<48;c++)a[r*48+c]=core._exp_get(t,r,c);
   return a;
  });
  postMessage({type:'result',id:d.id,parameters:p,tables},tables.map(t=>t.buffer));
 }catch(e){postMessage({type:'error',id:d.id,message:e.message});}
};
