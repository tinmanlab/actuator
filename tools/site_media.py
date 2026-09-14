#!/usr/bin/env python3
"""Publish actual-engine recordings without API-login or expiring artifact links."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--out',type=Path,default=Path('_site/media'));a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True)
summary=json.loads((a.evidence/'summary.json').read_text())
if not summary.get('accepted'):raise SystemExit('Refusing to publish failed MuJoCo evidence')
manifest={'kind':'Actual C++ + MuJoCo recordings; not browser physics','summary':summary,'videos':{}}
for case in ['tracking','disturbance','contact','impact','fault']:
 src=a.evidence/f'{case}_sub1/mujoco.mp4';dst=a.out/f'{case}.mp4'
 shutil.copy2(src,dst)
 subprocess.run(['ffmpeg','-y','-loglevel','error','-ss','1','-i',str(dst),'-frames:v','1','-q:v','3',str(a.out/f'{case}.jpg')],check=True)
 subprocess.run(['ffmpeg','-v','error','-i',str(dst),'-f','null','-'],check=True)
 manifest['videos'][case]={'sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'bytes':dst.stat().st_size}
(a.out/'evidence.json').write_text(json.dumps(manifest,indent=2)+'\n')
# A compact actual-engine animation for the GitHub README (GitHub strips iframes).
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(a.out/'contact.mp4'),'-t','5','-filter_complex','fps=8,scale=640:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=96[p];[s1][p]paletteuse=dither=bayer',str(a.out/'contact.gif')],check=True)
