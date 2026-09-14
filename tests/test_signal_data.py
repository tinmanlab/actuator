"""End-to-end data contracts: exercise the native model, not a hand-drawn curve."""
import csv, json, math, subprocess, sys, tempfile, unittest
from pathlib import Path

EXE = Path(sys.argv.pop()).resolve() if len(sys.argv)>1 and not sys.argv[-1].startswith('-') else None
class Signals(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(); cls.root=Path(cls.tmp.name)
        subprocess.run([str(EXE), str(cls.root)],check=True,capture_output=True,text=True)
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def rows(self,name):
        with (self.root/name).open() as f:
            return [{k:float(v) for k,v in r.items()} for r in csv.DictReader(f)]
    def test_all_traces_finite_and_ordered(self):
        for path in self.root.glob('*.csv'):
            r=self.rows(path.name); self.assertGreater(len(r),5,path.name)
            self.assertTrue(all(math.isfinite(v) for row in r for v in row.values()),path.name)
            if 'time_s' in r[0]: self.assertTrue(all(b['time_s']>a['time_s'] for a,b in zip(r,r[1:])),path.name)
    def test_svpwm_sweep_preserves_line_voltage(self):
        self.assertTrue((self.root/'modulation.csv').exists(), 'native modulation sweep missing')
        for r in self.rows('modulation.csv'):
            self.assertAlmostEqual(48*(r['duty_a']-r['duty_b']),r['phase_a_V']-r['phase_b_V'],places=4)
            self.assertTrue(all(.02 <= r['duty_'+p] <= .98 for p in 'abc'))
    def test_pwm_gates_and_floating_neutral(self):
        r=self.rows('pwm_edges.csv')
        self.assertTrue(any(x['high_b']==0 and x['low_b']==0 for x in r))
        for x in r:
            self.assertLess(abs(x['ia_A']+x['ib_A']+x['ic_A']),1e-7)
            self.assertLess(abs(x['va_V']+x['vb_V']+x['vc_V']),1e-7)
            self.assertTrue(all(not(x['high_'+p] and x['low_'+p]) for p in 'abc'))
    def test_same_state_bridge_power_closes(self):
        r=self.rows('velocity_power.csv')
        self.assertLess(max(abs(x['dc_W']-x['ac_W']-x['bridge_loss_W']) for x in r),1e-8)
        tail=[x for x in r if x['time_s']>=.8]
        self.assertGreater(sum(x['load_W'] for x in tail)/len(tail),2.8)
        self.assertTrue(all(x['copper_W']>=0 and x['friction_W']>=0 for x in r))
    def test_thermal_is_prescribed_current_not_closed_loop(self):
        r=self.rows('thermal.csv')
        self.assertAlmostEqual(r[0]['winding_C'],25,places=6)
        self.assertGreater(r[-1]['winding_C'],r[-1]['case_C'])
        self.assertGreater(r[-1]['case_C'],25)
        self.assertGreater(r[-1]['resistance_ohm'],r[0]['resistance_ohm'])
        self.assertAlmostEqual(r[0]['copper_W'],1.5*.08*12**2,places=5)
        info=json.loads((self.root/'experiment.json').read_text())
        self.assertEqual(info['thermal_kind'],'prescribed current; not closed-loop drive')
    def test_efficiency_is_not_reported_at_zero_power_or_regeneration(self):
        r=self.rows('regeneration_power.csv')
        self.assertLess(min(x['dc_W'] for x in r),-1)
        meta=json.loads((self.root/'experiment.json').read_text())
        eta=meta['motoring']['efficiency']
        self.assertGreater(eta,0);self.assertLess(eta,1)
        self.assertLess(abs(meta['motoring']['stored_energy_rate_W']),.05)
        self.assertIn('synthetic',meta['claim'])
if __name__=='__main__': unittest.main()
