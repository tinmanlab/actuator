/* Reading aids only: no simulation state, hardware access, or remote API calls. */
(() => {
 const controls=[...document.querySelectorAll('[data-stage]')];
 const details=[...document.querySelectorAll('[data-detail]')];
 function select(stage,updateHash=false){
  if(!details.some(d=>d.dataset.detail===stage))return;
  details.forEach(d=>{d.hidden=d.dataset.detail!==stage;});
  controls.forEach(c=>{if(c.tagName==='BUTTON')c.setAttribute('aria-pressed',String(c.dataset.stage===stage));});
  if(updateHash)history.replaceState(null,'','#stage-'+stage);
 }
 for(const c of controls)c.addEventListener('click',e=>{e.preventDefault();select(c.dataset.stage,true);});
 const stage=location.hash.replace('#stage-','');select(details.some(d=>d.dataset.detail===stage)?stage:'outer');
 window.addEventListener('hashchange',()=>select(location.hash.replace('#stage-','')));
 const input=document.getElementById('electrical-angle');
 function drawFrame(){
  const deg=Number(input.value),angle=deg*Math.PI/180;
  // amplitude-invariant inverse transforms, id=0, iq=4 A, as in math.hpp
  const alpha=-4*Math.sin(angle),beta=4*Math.cos(angle);
  const a=alpha,b=-alpha/2+Math.sqrt(3)*beta/2,c=-alpha/2-Math.sqrt(3)*beta/2;
  document.getElementById('dq-frame').setAttribute('transform',`rotate(${-deg} 155 140)`);
  document.getElementById('angle-readout').textContent=deg+'°';
  document.getElementById('phase-values').textContent=`ia = ${a.toFixed(3)} A · ib = ${b.toFixed(3)} A · ic = ${c.toFixed(3)} A`;
 }
 if(input){input.addEventListener('input',drawFrame);drawFrame();}
 for(const button of document.querySelectorAll('[data-copy]'))button.addEventListener('click',async()=>{
  const code=document.getElementById(button.dataset.copy);
  try{await navigator.clipboard.writeText(code.textContent);button.textContent='Copied';}
  catch{const range=document.createRange();range.selectNodeContents(code);const selection=getSelection();selection.removeAllRanges();selection.addRange(range);button.textContent='Selected — copy manually';}
 });
 fetch('build.json').then(r=>{if(!r.ok)throw Error('missing manifest');return r.json();}).then(b=>{
  const sha=b.source_commit;
  document.getElementById('guide-version').textContent='Source '+sha.slice(0,12)+' · synthetic software-in-the-loop';
  if(/^[a-f0-9]{40}$/.test(sha))for(const a of document.querySelectorAll('[data-source]'))a.href='https://github.com/tinmanlab/actuator/blob/'+sha+'/'+a.dataset.source;
 }).catch(()=>{document.getElementById('guide-version').textContent='Local preview · source commit unavailable';});
})();
