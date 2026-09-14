#!/usr/bin/env python3
"""Package native experiment data. No motor equations or invented chart values."""
import argparse, csv, hashlib, json, math, subprocess, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
    with p.open() as f: rows=[{k:float(v) for k,v in r.items()} for r in csv.DictReader(f)]
    if not rows or any(not math.isfinite(v) for r in rows for v in r.values()):
        raise ValueError(f'Invalid native trace: {p}')
    return rows

def build(exe,out):
    out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp); subprocess.run([str(exe.resolve()),str(tmp)],check=True)
        data={'meta':json.loads((tmp/'experiment.json').read_text()),'traces':{}}
        windows={'current':(0,.04),'gate_off':(.077,.083),'derating':(0,.06),'pwm_edges':(0,1),'thermal':(0,601)}
        for name,(lo,hi) in windows.items():
            rows=read(tmp/(name+'.csv'))
            data['traces'][name]=[r for r in rows if lo<=r['time_s']<=hi]
        data['traces']['modulation']=read(tmp/'modulation.csv')
        # Curves use 1 ms block averages; raw 50 us rows remain in the archive.
        # Never downsample the gate trace or imply this view shows switching peaks.
        for name in ['velocity','friction','regeneration']:
            rows=read(tmp/(name+'_power.csv')); groups=[]
            for i in range(0,len(rows),20):
                g=rows[i:i+20];v={k:sum(r[k] for r in g)/len(g) for k in g[0]}
                v['time_s']=g[-1]['time_s'];groups.append(v)
            data['traces'][name]=groups
        data['meta']['display_power']='1 ms block means of native 50 us period-averaged diagnostics; not switching peaks'
        data['meta']['files']={p.name:sha(p) for p in sorted(tmp.iterdir())}
        data['meta']['native_executable_sha256']=sha(exe)
        data['meta']['source_sha256']={str(p.relative_to(ROOT)):sha(p) for folder in ['src','include','examples'] for p in sorted((ROOT/folder).rglob('*')) if p.is_file() and p.suffix in ['.cpp','.hpp','.h']}
        with zipfile.ZipFile(out/'native-signals.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for p in sorted(tmp.iterdir()): z.write(p,p.name)
        (out/'signals.json').write_text(json.dumps(data,separators=(',',':'),allow_nan=False)+'\n')
        (out/'signals-manifest.json').write_text(json.dumps(data['meta'],indent=2)+'\n')
        print('Signal data:',out,'motoring ratio:',data['meta']['motoring']['efficiency'])
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=ROOT/'build/qdd_signal_lab');p.add_argument('--out',type=Path,default=ROOT/'_site/data');a=p.parse_args();build(a.exe,a.out)
