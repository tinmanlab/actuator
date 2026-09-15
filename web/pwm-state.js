/* Display semantics for existing native gate samples. This is not a motor solver. */
export function decodeBridge(row, currents, bus, enabled=true){
 const invalid={kind:'invalid',label:'Invalid sample',alpha:null,beta:null,paths:['none','none','none']};
 if(!row||row.length<11||!currents||currents.length!==3||!currents.every(Number.isFinite)||!Number.isFinite(bus)||bus<=0)return invalid;
 const bits=row.slice(4,10);
 if(!bits.every(v=>v===0||v===1)||[0,2,4].some(k=>bits[k]&&bits[k+1]))return invalid;
 const paths=currents.map((i,k)=>Math.abs(i)<.02?'none':bits[2*k]?'high':bits[2*k+1]?'low':i>0?'low-diode':'high-diode');
 if(!enabled)return {kind:'off',label:'Gates inhibited',alpha:null,beta:null,paths};
 if([0,2,4].some(k=>!bits[k]&&!bits[k+1]))return {kind:'deadtime',label:'Dead time',alpha:null,beta:null,paths};
 const h=[bits[0],bits[2],bits[4]],label=h.join('');
 return {kind:label==='000'||label==='111'?'zero':'active',label,
  alpha:bus*(2*h[0]-h[1]-h[2])/3,beta:bus*(h[1]-h[2])/Math.sqrt(3),paths};
}
export function selectSample(rows,phase){
 if(!rows?.length||!Number.isFinite(phase))return null;
 let lo=0,hi=rows.length-1;
 while(lo<hi){const mid=Math.ceil((lo+hi)/2);if(rows[mid][0]<=phase)lo=mid;else hi=mid-1;}
 return rows[lo];
}
export function edgeTimes(rows){
 return (rows||[]).filter((r,k)=>k===0||r.slice(4,10).some((v,j)=>v!==rows[k-1][j+4])).map(r=>r[0]);
}
export function advanceCursor(phase,elapsedMs,cycleMs){
 const next=phase+Math.max(0,elapsedMs)/cycleMs*50e-6;
 return {phase:next%(50e-6),wrapped:next>=50e-6};
}
