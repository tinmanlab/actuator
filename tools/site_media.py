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
 # Keep the original H.264 MP4 and add a standards-based VP9 alternative.
 # Both derive from the same accepted actual-engine recording, never a pose animation.
 webm=a.out/f'{case}.webm'
 subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(dst),'-an','-c:v','libvpx-vp9','-crf','32','-b:v','0','-deadline','good','-cpu-used','4','-pix_fmt','yuv420p',str(webm)],check=True)
 subprocess.run(['ffmpeg','-v','error','-i',str(webm),'-f','null','-'],check=True)
 def probe(path):
  result=subprocess.run(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=codec_name,width,height,nb_read_frames','-of','json',str(path)],check=True,capture_output=True,text=True)
  return json.loads(result.stdout)['streams'][0]
 original,portable=probe(dst),probe(webm)
 if portable['codec_name']!='vp9' or any(original[k]!=portable[k] for k in ['width','height','nb_read_frames']):
  raise SystemExit(f'{case}: portable encoding changed frame count or dimensions')
 subprocess.run(['ffmpeg','-y','-loglevel','error','-ss','1','-i',str(dst),'-frames:v','1','-q:v','3',str(a.out/f'{case}.jpg')],check=True)
 subprocess.run(['ffmpeg','-v','error','-i',str(dst),'-f','null','-'],check=True)
 manifest['videos'][case]={'sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'bytes':dst.stat().st_size,'probe':original,'webm':{'sha256':hashlib.sha256(webm.read_bytes()).hexdigest(),'bytes':webm.stat().st_size,'probe':portable}}
(a.out/'evidence.json').write_text(json.dumps(manifest,indent=2)+'\n')
# A compact actual-engine animation for the GitHub README (GitHub strips iframes).
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(a.out/'contact.mp4'),'-t','5','-filter_complex','fps=8,scale=640:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=96[p];[s1][p]paletteuse=dither=bayer',str(a.out/'contact.gif')],check=True)
