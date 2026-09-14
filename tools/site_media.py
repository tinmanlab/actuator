#!/usr/bin/env python3
"""Publish accepted event excerpts, full recordings and a sequential README tour.
The video loops restart an experiment excerpt; frames are never reversed.
"""
import argparse,csv,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
from event_media import event_window

CASES=('tracking','disturbance','contact','impact','fault')
LABELS=('Track a target','Reject a load pulse','Meet a stop','Absorb an impact','Trip the driver')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def ff(*args):subprocess.run(['ffmpeg','-y','-loglevel','error',*map(str,args)],check=True)
def probe(path):
 r=subprocess.run(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries',
   'stream=codec_name,width,height,nb_read_frames:format=duration','-of','json',str(path)],check=True,capture_output=True,text=True)
 d=json.loads(r.stdout);return {**d['streams'][0],'duration_s':float(d['format']['duration'])}
def main():
 p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--out',type=Path,default=Path('_site/media'));a=p.parse_args()
 summary=json.loads((a.evidence/'summary.json').read_text())
 if not summary.get('accepted'):raise SystemExit('Refusing to publish failed MuJoCo evidence')
 a.out.mkdir(parents=True,exist_ok=True);(a.out/'full').mkdir(exist_ok=True)
 manifest={'kind':'Actual C++ + MuJoCo event excerpts; replay resets, not browser physics','summary':summary,'videos':{}}
 with tempfile.TemporaryDirectory() as temp:
  temp=Path(temp);segments=[]
  for case,label in zip(CASES,LABELS):
   src=a.evidence/f'{case}_sub1/mujoco.mp4';rep=json.loads((src.parent/'summary.json').read_text())
   trace=src.parent/'trace.csv'
   if not rep.get('accepted') or sha(trace)!=rep['physics_trace_sha256']:raise ValueError(f'{case}: trace provenance mismatch')
   with trace.open() as s:rows=list(csv.DictReader(s))
   start,end,event=event_window(case,rows)
   # run.py supplies the exact simulation-seconds/video-seconds ratio. Historical
   # evidence lacks this key and must be regenerated; never infer it from a name.
   speed=rep['playback_speed']
   if not isinstance(speed,(int,float)) or not 0<speed<=10:raise ValueError('Invalid playback rate')
   original=probe(src);full=a.out/f'full/{case}.mp4';shutil.copy2(src,full)
   dst=a.out/f'{case}.mp4'
   ff('-ss',start/speed,'-i',src,'-t',(end-start)/speed,'-an','-c:v','libx264','-crf',22,'-pix_fmt','yuv420p','-movflags','+faststart',dst)
   webm=a.out/f'{case}.webm';ff('-i',dst,'-an','-c:v','libvpx-vp9','-crf',32,'-b:v',0,'-deadline','good','-cpu-used',4,'-pix_fmt','yuv420p',webm)
   clip,portable=probe(dst),probe(webm)
   if portable['codec_name']!='vp9' or any(clip[k]!=portable[k] for k in ['width','height','nb_read_frames']):raise ValueError('Encoding changed frame count/dimensions')
   if clip['duration_s']>=original['duration_s'] or not .2<clip['duration_s']<4:raise ValueError('Excerpt not shorter than source')
   for movie in (dst,webm):ff('-i',movie,'-f','null','-')
   ff('-ss',min(.4,clip['duration_s']/2),'-i',dst,'-frames:v',1,'-q:v',3,a.out/f'{case}.jpg')
   manifest['videos'][case]={'sha256':sha(dst),'bytes':dst.stat().st_size,'probe':clip,
    'webm':{'sha256':sha(webm),'bytes':webm.stat().st_size,'probe':portable},
    'source_sha256':sha(src),'source_probe':original,'trace_sha256':sha(trace),'playback_speed':speed,
    'event_sim_s':event,'window_sim_s':[start,end],'full_recording':f'full/{case}.mp4','replay':'restart excerpt; no reverse frames'}
   segment=temp/f'{case}.mp4';segments.append(segment)
   # Case-specific caption retains provenance after the original overlay is scaled.
   vf=f"fps=10,scale=800:450:flags=lanczos,drawbox=x=0:y=398:w=iw:h=52:color=black@0.82:t=fill,drawtext=text='{label}  |  Actual MuJoCo excerpt - replay':x=16:y=414:fontsize=18:fontcolor=white"
   ff('-i',dst,'-vf',vf,'-an','-c:v','libx264','-pix_fmt','yuv420p',segment)
  concat=temp/'clips.txt';concat.write_text(''.join(f"file '{s}'\n" for s in segments))
  tour=temp/'tour.mp4';ff('-f','concat','-safe',0,'-i',concat,'-c','copy',tour)
  gif=a.out/'feature-tour.gif';ff('-i',tour,'-filter_complex','split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse=dither=bayer:bayer_scale=4','-loop',0,gif)
  if gif.stat().st_size>4*1024*1024:raise ValueError('README GIF exceeds 4 MiB budget')
  ff('-i',gif,'-f','null','-')
  manifest['tour']={'sha256':sha(gif),'bytes':gif.stat().st_size,'probe':probe(gif),'order':list(CASES),'layout':'sequential, not a simultaneous grid'}
  # Keep the existing educational contact GIF URL valid.
  ff('-i',a.out/'contact.mp4','-filter_complex','fps=8,scale=640:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse=dither=bayer','-loop',0,a.out/'contact.gif')
 (a.out/'evidence.json').write_text(json.dumps(manifest,indent=2,allow_nan=False)+'\n')
 print(json.dumps({k:v['window_sim_s'] for k,v in manifest['videos'].items()}))
if __name__=='__main__':main()
