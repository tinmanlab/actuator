#!/usr/bin/env python3
"""Deterministic illustrative QDD geometry, SI metres. Not vendor CAD or a PCB.
Only zero-mass, non-contact decoration is replaced. The original hinge/physical
mass-bearing geometry is retained to avoid changing the mechanical plant for art.
No external assets or downloads. Runtime MuJoCo only needs the generated OBJ/XML.
"""
from __future__ import annotations
from pathlib import Path
import math
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'examples/mujoco'
ASSETS=OUT/'assets'
def txt(x): return ' '.join(f'{v:.10g}' for v in x)
def mesh_file(name, vertices, faces):
    # Use outward-oriented closed surfaces; no flat painted bores.
    vol=sum(sum(vertices[a][k]*(vertices[b][(k+1)%3]*vertices[c][(k+2)%3]-vertices[b][(k+2)%3]*vertices[c][(k+1)%3]) for k in range(3)) for a,b,c in faces)/6
    if vol<0: faces=[(a,c,b) for a,b,c in faces]
    p=ASSETS/f'{name}.obj'
    p.write_text('# Procedural illustrative QDD component; metres, Y axis\n'+''.join('v '+txt(v)+'\n' for v in vertices)+''.join('f '+txt([i+1 for i in f])+'\n' for f in faces))
    return p

def ring(name,ri,ro,y0,y1,bevel=.0003,n=96):
    b=min(bevel,(ro-ri)/3,(y1-y0)/3)
    profile=[(ri,y0+b),(ri+b,y0),(ro-b,y0),(ro,y0+b),(ro,y1-b),(ro-b,y1),(ri+b,y1),(ri,y1-b)]
    v=[(r*math.cos(2*math.pi*i/n),y,r*math.sin(2*math.pi*i/n)) for r,y in profile for i in range(n)]
    f=[]
    for j in range(len(profile)):
        for i in range(n):
            a=j*n+i;b=j*n+(i+1)%n;c=((j+1)%len(profile))*n+(i+1)%n;d=((j+1)%len(profile))*n+i
            f.extend([(a,b,c),(a,c,d)])
    mesh_file(name,v,f)

def coil(name):
    # Rectangular annulus in the Y/Z plane, extruded radially along X.
    loops=[]
    for x,hy,hz in [(-.004,.018,.006),(.004,.018,.006),(-.004,.014,.003),(.004,.014,.003)]:
        loops += [(x,-hy,-hz),(x,hy,-hz),(x,hy,hz),(x,-hy,hz)]
    faces=[]
    for i in range(4):
        j=(i+1)%4
        for a,b,c,d in [(i,j,4+j,4+i),(8+i,12+i,12+j,8+j),(i,8+i,8+j,j),(4+i,4+j,12+j,12+i)]:
            faces.extend([(a,b,c),(a,c,d)])
    mesh_file(name,loops,faces)

def main():
    ASSETS.mkdir(parents=True,exist_ok=True)
    root=ET.parse(ROOT/'examples/mujoco/bench_base.xml').getroot()
    root.set('model','QDD actuator bench — illustrative assembly, unchanged inertia')
    asset=root.find('asset');world=root.find('worldbody')
    for name,rgba in [('shell','0.12 0.15 0.19 1'),('steel','0.42 0.48 0.54 1'),('silver','0.7 0.74 0.77 1'),('insulator','0.08 0.09 0.1 1'),('phase_u','0.85 0.25 0.13 1'),('phase_v','0.92 0.69 0.17 1'),('phase_w','0.13 0.38 0.72 1'),('magnet_n','0.3 0.34 0.4 1'),('magnet_s','0.4 0.44 0.49 1')]:
        ET.SubElement(asset,'material',name=name,rgba=rgba,specular='.6',shininess='.45')
    specs={
      'motor_case_mesh':(.046,.053,0,.064),
      'gear_case_mesh':(.039,.053,-.050,0),
      'front_face_mesh':(.025,.053,-.059,-.050),
      'rear_cap_mesh':(.008,.053,.064,.072),
      'front_bearing_mesh':(.018,.025,-.060,-.050),
      'stator_stack_mesh':(.013,.024,.014,.050),
      'rotor_shell_mesh':(.041,.044,.012,.052),
      'rotor_back_mesh':(.012,.044,.050,.054),
      'flange_mesh':(.006,.035,-.012,.012),
      'bolt_head_mesh':(.0014,.0034,-.0018,.0018),
      'encoder_ring_mesh':(.004,.010,.060,.064),
    }
    for name,pars in specs.items():
        ring(name,*pars)
        ET.SubElement(asset,'mesh',name=name,file=f'assets/{name}.obj')
    coil('winding_mesh');ET.SubElement(asset,'mesh',name='winding_mesh',file='assets/winding_mesh.obj')
    def geom(parent,name,kind='box',material='silver',**kw):
        data=dict(name=name,type=kind,material=material,mass='0',contype='0',conaffinity='0',group='1')
        data.update({k:txt(v) if isinstance(v,(list,tuple)) else str(v) for k,v in kw.items()})
        return ET.SubElement(parent,'geom',**data)
    def box(parent,name,pos,size,mat='silver',**kw):return geom(parent,name,material=mat,pos=pos,size=size,**kw)
    def cyl(parent,name,pos,r,half,mat='silver',**kw):return geom(parent,name,'cylinder',mat,pos=pos,size=[r,half],quat=[2**-.5,2**-.5,0,0],**kw)
    def mesh(parent,name,shape,pos=(0,0,.6),mat='silver',**kw):return geom(parent,name,'mesh',mat,mesh=shape,pos=pos,**kw)
    for g in list(world):
        if g.tag=='geom' and g.get('name') not in ('floor','workbench','stopper'):world.remove(g)
    # Grounded fixture. Rear plate touches both shell rear cap and vertical column.
    for i,(x,y) in enumerate([(-.44,-.22),(.44,-.22),(-.44,.22),(.44,.22)]):
        geom(world,f'bench_foot_{i}','cylinder','dark',pos=[x,y,.0625],size=[.03,.0625])
    box(world,'mount_foot',[0,.10,.182],[.12,.075,.007],'steel')
    box(world,'mount_column',[0,.115,.373],[.045,.02,.190],'steel')
    box(world,'motor_mount_plate',[0,.078,.6],[.069,.006,.069],'steel')
    for i,(x,z) in enumerate([(-.057,.543),(.057,.543),(-.057,.657),(.057,.657)]):
        mesh(world,f'rear_mount_bolt_{i}','bolt_head_mesh',(x,.070,z),'dark')
    for name,shape,mat in [('motor_housing','motor_case_mesh','shell'),('gear_housing','gear_case_mesh','shell'),('front_face','front_face_mesh','silver'),('rear_cap','rear_cap_mesh','silver'),('front_bearing','front_bearing_mesh','steel'),('stator_stack','stator_stack_mesh','steel'),('encoder_ring','encoder_ring_mesh','pcb')]:
        mesh(world,name,shape,mat=mat)
    # Fixed stator stack and 12 windings. Four coils per phase are decorative;
    # electromechanical equations still use the measured/nominal dq parameters.
    for i in range(12):
        phi=i*2*math.pi/12;q=[math.cos(phi/2),0,-math.sin(phi/2),0]
        box(world,f'stator_tooth_{i}',[.0305*math.cos(phi),.032,.6+.0305*math.sin(phi)],[.0066,.013,.0025],'steel',quat=q)
        mesh(world,f'winding_{i}','winding_mesh',(.0285*math.cos(phi),.032,.6+.0285*math.sin(phi)),'copper',quat=q)
    for i in range(6):
        phi=2*math.pi*i/6
        mesh(world,f'case_bolt_{i}','bolt_head_mesh',(.047*math.cos(phi),-.061,.6+.047*math.sin(phi)),'dark')
    # Realistic component scale, not a 54 mm-wide MCU package.
    box(world,'pcb',[.29,.025,.199],[.065,.048,.001],'pcb')
    box(world,'mcu',[.292,.04,.202],[.007,.007,.0015],'dark')
    for i in range(8):
        for s in (-1,1):box(world,f'mcu_pin_{i}_{s}',[.292+s*.008,.034+i*.0016,.201],[.001,.00035,.0003])
    for p,x in zip('abc',[.250,.273,.296]):
        for side,y in [('high',-.004),('low',.009)]:box(world,f'fet_{p}_{side}',[x,y,.202],[.004,.004,.0015],'dark')
        box(world,f'shunt_{p}',[x,-.013,.2015],[.003,.0015,.0005],'steel')
        box(world,f'gate_driver_{p}',[x,.020,.2015],[.0025,.0025,.0005],'dark')
    for i,y in enumerate([.012,.036]):
        geom(world,f'capacitor_{i}','cylinder','dark',pos=[.335,y,.211],size=[.006,.011])
        geom(world,f'capacitor_top_{i}','cylinder','silver',pos=[.335,y,.2221],size=[.0057,.0001])
    for i,(x,y) in enumerate([(.232,-.016),(.348,-.016),(.232,.066),(.348,.066)]):
        geom(world,f'pcb_standoff_{i}','cylinder','steel',pos=[x,y,.1865],size=[.0028,.0115])
        geom(world,f'pcb_screw_{i}','cylinder','silver',pos=[x,y,.201],size=[.003,.001])
    box(world,'phase_connector',[.238,-.007,.204],[.005,.016,.004],'dark')
    box(world,'dc_connector',[.348,-.003,.204],[.004,.008,.004],'phase_v')
    box(world,'signal_connector',[.309,.063,.203],[.010,.003,.002],'dark')
    for i,p in enumerate('uvw'):
        k=(i-1)*.005
        path=[(.235,-.017+i*.009,.208),(.18,.024+k,.215),(.10,.09+k,.28),(.075,.096+k,.46),(.069,.046+k,.551),(.041,.032+k,.566)]
        for j,(a,b) in enumerate(zip(path,path[1:])):geom(world,f'phase_{p}_{j}','capsule',f'phase_{p}',fromto=[*a,*b],size=[.0019])
    for j,(a,b) in enumerate(zip([(.31,.066,.205),(.18,.11,.21),(.04,.123,.4),(0,.083,.6)],[(.18,.11,.21),(.04,.123,.4),(0,.083,.6),(0,.073,.6)])):
        geom(world,f'encoder_cable_{j}','capsule','insulator',fromto=[*a,*b],size=[.0021])
    box(world,'cable_clip',[.075,.096,.46],[.008,.012,.004],'dark')
    # A bolted-down stop, rather than a floating orange box.
    stop_x=-.32*math.sin(.60)-.031-.028
    stopper=world.find("geom[@name='stopper']");stopper.set('pos',txt([stop_x,-.09,.36]));stopper.set('size',txt([.028,.045,.07]))
    box(world,'stop_foot',[stop_x,-.09,.185],[.065,.070,.010],'steel')
    box(world,'stop_column',[stop_x,-.09,.25],[.019,.025,.055],'steel')
    # Dynamic rotor remains entirely inside the fixed motor shell.
    rotor=world.find("body[@name='rotor']")
    for g in list(rotor.findall('geom')):rotor.remove(g)
    mesh(rotor,'rotor_shell','rotor_shell_mesh',(0,0,0),'steel')
    mesh(rotor,'rotor_back','rotor_back_mesh',(0,0,0),'steel')
    cyl(rotor,'rotor_shaft',(0,.025,0),.011,.031,'steel')
    for i in range(14):
        phi=i*2*math.pi/14
        box(rotor,f'magnet_{i}',[.040*math.cos(phi),.032,.040*math.sin(phi)],[.0015,.018,.0073],'magnet_n' if i%2==0 else 'magnet_s',quat=[math.cos(phi/2),0,-math.sin(phi/2),0])
    output=world.find("body[@name='output']")
    # Retain original mass/inertia/collision sources, hide their crude surfaces.
    for g in output.findall('geom'):g.set('rgba','0 0 0 0');g.set('group','3')
    geom(output,'output_shaft','cylinder','steel',fromto=[0,.008,0,0,.040,0],size=[.01795])
    mesh(output,'output_flange','flange_mesh',(0,0,0),'silver')
    cyl(output,'flange_center_cap',(0,-.0125,0),.0075,.0015,'dark')
    for i in range(6):
        phi=2*math.pi*i/6
        mesh(output,f'output_bolt_{i}','bolt_head_mesh',(.026*math.cos(phi),-.015,.026*math.sin(phi)),'dark')
    # A rectangular machined link on the flange, not a free-floating capsule.
    box(output,'link_visual',[0,-.004,-.17],[.018,.009,.15],'arm')
    box(output,'link_root',[0,-.004,-.025],[.026,.009,.025],'arm')
    cyl(output,'tip_visual',[0,-.004,-.32],.03,.016,'silver')
    cyl(output,'tip_cap',[0,-.022,-.32],.015,.002,'dark')
    ET.indent(root,space='  ')
    ET.ElementTree(root).write(OUT/'bench.xml',encoding='unicode',xml_declaration=False)
    print('Wrote illustrative assembly with',len(list(root.iter('geom'))),'geoms;',len(specs)+1,'closed OBJ meshes')
if __name__=='__main__':main()
