"""Structural asset regression tests. These do NOT run MuJoCo dynamics."""
from pathlib import Path
import math
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
class ModelContracts(unittest.TestCase):
    def setUp(self):
        self.tree = ET.parse(ROOT/'examples/mujoco/bench.xml')
        self.root = self.tree.getroot()
        self.parent = {c:p for p in self.root.iter() for c in p}
    def geom(self, name):
        g = self.root.find(f".//geom[@name='{name}']")
        self.assertIsNotNone(g, f"missing geometric feature: {name}")
        return g
    def owner(self, el):
        while el in self.parent:
            el = self.parent[el]
            if el.tag in ('body', 'worldbody'): return el.get('name', 'world')
    def test_output_supported_by_coaxial_shaft(self):
        g = self.geom('output_shaft')
        self.assertEqual(self.owner(g), 'output')
        pts = list(map(float, g.get('fromto').split()))
        self.assertEqual([pts[0],pts[2],pts[3],pts[5]], [0,0,0,0])
        self.assertLessEqual(min(pts[1],pts[4]), .010)
        self.assertGreaterEqual(max(pts[1],pts[4]), .037)
        self.geom('front_bearing')
    def test_stop_is_tangent_at_declared_angle(self):
        g=self.geom('stopper');pos=list(map(float,g.get('pos').split()));half=list(map(float,g.get('size').split()))
        tip_x=-.32*math.sin(.60)
        self.assertAlmostEqual(pos[0]+half[0],tip_x-.031,places=8)
        self.assertLessEqual(pos[1]-half[1],-.09-.031)
        self.assertGreaterEqual(pos[1]+half[1],-.09+.031)

    def test_windings_are_fixed_not_rotating(self):
        windings = [g for g in self.root.iter('geom') if (g.get('name') or '').startswith('winding_')]
        self.assertEqual(len(windings),12)
        self.assertTrue(all(self.owner(g)=='world' for g in windings))
    def test_magnets_belong_to_rotor(self):
        magnets = [g for g in self.root.iter('geom') if (g.get('name') or '').startswith('magnet_')]
        self.assertEqual(len(magnets),14)
        self.assertTrue(all(self.owner(g)=='rotor' for g in magnets))
    def test_three_phase_cables_and_six_switches(self):
        for p in 'uvw': self.geom(f'phase_{p}_0')
        for phase in 'abc':
            for side in ('high','low'): self.geom(f'fet_{phase}_{side}')
    def test_decoration_does_not_add_inertia_or_contacts(self):
        decorations = [g for g in self.root.iter('geom') if g.get('group')=='1']
        self.assertGreater(len(decorations),60)
        for g in decorations:
            self.assertEqual(g.get('mass'), '0', g.get('name'))
            self.assertEqual(g.get('contype'),'0',g.get('name'))
            self.assertEqual(g.get('conaffinity'),'0',g.get('name'))
    def test_physical_inertia_sources_unchanged(self):
        old = ET.parse(ROOT/'examples/mujoco/bench_base.xml')
        for name in ('hub','link','tip'):
            a=old.find(f".//geom[@name='{name}']"); b=self.geom(name)
            for key in ('type','size','fromto','pos','mass'):
                self.assertEqual(a.get(key),b.get(key),(name,key))
        for name in ('rotor_joint','output_joint'):
            self.assertEqual(old.find(f".//joint[@name='{name}']").attrib,self.root.find(f".//joint[@name='{name}']").attrib)
    def test_mount_reaches_motor_and_bench(self):
        self.geom('motor_mount_plate'); self.geom('mount_foot'); self.geom('rear_mount_bolt_0')
    def test_hollow_mesh_has_real_bore(self):
        asset = self.root.find(".//mesh[@name='front_bearing_mesh']")
        self.assertIsNotNone(asset)
        path=ROOT/'examples/mujoco'/asset.get('file')
        vertices=[]; faces=[]
        for line in path.read_text().splitlines():
            if line.startswith('v '): vertices.append(list(map(float,line.split()[1:])))
            if line.startswith('f '): faces.append([int(x.split('/')[0])-1 for x in line.split()[1:]])
        self.assertGreater(len(faces),32)
        self.assertTrue(all(math.hypot(v[0],v[2])>=.0179 for v in vertices))
        edges={}
        for f in faces:
            for a,b in zip(f,f[1:]+f[:1]):
                key=tuple(sorted((a,b)));edges[key]=edges.get(key,0)+1
        self.assertTrue(all(n==2 for n in edges.values()),'closed manifold ring, not painted hole')

if __name__=='__main__':unittest.main()

class HarnessContracts(unittest.TestCase):
 def test_power_source_and_closed_paths(self):
  root=ET.parse(ROOT/'examples/mujoco/bench.xml').getroot()
  for name in ('dc_supply','supply_terminal','motor_terminal','phase_connector','signal_connector'):
   self.assertIsNotNone(root.find(f".//geom[@name='{name}']"),name)
  # Centerlines, including cable radius, may not cross the column, base or PCB.
  world=root.find('worldbody')
  for prefix in ('phase_u_','phase_v_','phase_w_','encoder_cable_','dc_positive_','dc_return_'):
   cables=[g for g in world.findall('geom') if g.get('name','').startswith(prefix)]
   self.assertGreater(len(cables),1,prefix)
   last=None
   for g in cables:
    pt=list(map(float,g.get('fromto').split()));a,b=pt[:3],pt[3:];radius=float(g.get('size'))
    if last is not None:self.assertEqual(a,last,prefix+' broken cable')
    last=b
    for name in ('mount_column','mount_foot','workbench','pcb','dc_supply'):
     box=world.find(f"geom[@name='{name}']")
     pos=list(map(float,box.get('pos').split()));half=list(map(float,box.get('size').split()))
     for t in [i/100 for i in range(101)]:
      p=[u+(v-u)*t for u,v in zip(a,b)]
      inside=all(abs(p[k]-pos[k])<half[k]+radius-1e-5 for k in range(3))
      self.assertFalse(inside,(g.get('name'),name,p))
    for t in [i/100 for i in range(101)]:
     p=[u+(v-u)*t for u,v in zip(a,b)];rad=math.hypot(p[0],p[2]-.6)
     self.assertFalse(.072-radius<p[1]<.084+radius and .055-radius<rad<.085+radius,(g.get('name'),'mount bore'))
