#!/usr/bin/env python3
"""Package same-origin static site and reuse the reviewed MJCF geometry recipe.
No physics is reimplemented here. Run after npm dependencies and Emscripten build.
"""
from pathlib import Path
import argparse, json, shutil, subprocess, sys, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'_site');a=p.parse_args();out=a.out
 out.mkdir(parents=True,exist_ok=True)
 for f in (ROOT/'web').iterdir():
  if f.suffix in ['.html','.css','.js']:shutil.copy2(f,out/f.name)
 (out/'vendor').mkdir(exist_ok=True)
 vendor=ROOT/'web/node_modules/three'
 shutil.copy2(vendor/'build/three.module.js',out/'vendor/three.module.js')
 orbit=(vendor/'examples/jsm/controls/OrbitControls.js').read_text().replace("from 'three'","from './three.module.js'")
 (out/'vendor/OrbitControls.js').write_text(orbit)
 shutil.copy2(vendor/'LICENSE',out/'vendor/THREE-LICENSE.txt')
 subprocess.run([sys.executable,str(ROOT/'tools/build_bench_model.py')],check=True)
 base=ROOT/'examples/mujoco';root=ET.parse(base/'bench.xml').getroot()
 def numeric(s):return [float(x) for x in s.split()]
 meshes={}
 for e in root.findall('asset/mesh'):
  v=[];ind=[]
  for line in (base/e.get('file')).read_text().splitlines():
   if line.startswith('v '):v.extend(numeric(line[2:]))
   elif line.startswith('f '):ind.extend(int(t.split('/')[0])-1 for t in line[2:].split())
  meshes[e.get('name')]={'vertices':v,'indices':ind}
 materials={e.get('name'):e.attrib for e in root.findall('asset/material')}
 def node(e):return {'tag':e.tag,'a':e.attrib,'children':[node(c) for c in e if c.tag in ('body','geom','joint')]}
 (out/'scene.json').write_text(json.dumps({'materials':materials,'meshes':meshes,'world':node(root.find('worldbody'))},separators=(',',':')))
 (out/'media').mkdir(exist_ok=True)
 docs=out/'guides';docs.mkdir(exist_ok=True)
 for f in (ROOT/'docs').glob('*.md'):shutil.copy2(f,docs/f.name)
 for f in (ROOT/'docs/tutorials').glob('*.md'):
  shutil.copy2(f,docs/f.name)
 sha=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,text=True,capture_output=True)
 (out/'build.json').write_text(json.dumps({'source_commit':sha.stdout.strip() if sha.returncode==0 else 'local', 'engine':'C++ control/plant/protection -> WASM','physics':'averaged inverter; two-inertia 1-axis QDD; unilateral angular stop','telemetry_hz':1000,'control_hz':20000},indent=2))
 (out/'.nojekyll').touch()
 print('Static site prepared:',out)
if __name__=='__main__':main()
