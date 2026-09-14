"""Acceptance logic fixtures, NOT engine-execution evidence."""
from pathlib import Path
import sys,unittest,copy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'examples/mujoco'))
import checks
class Checks(unittest.TestCase):
    def sample(self):
        return dict(engine='MuJoCo',version='3.3.5',case='tracking',completed=True,
            finite=True,warnings=0,final_fault=0,final_output_rad=.82,target_rad=.85,
            max_phase_current_A=12.,intended_contact_steps=0,max_penetration_m=0.,
            max_intended_contact_force_N=0.,final_gate_enabled=True,final_iq_A=1.,
            driver_fault_time_s=None,physics_trace_sha256='a'*64,mjcf_sha256='b'*64,
            library_sha256='c'*64,simulated_seconds=2.,expected_seconds=2.)
    def test_accept_tracking_fixture(self):self.assertTrue(checks.evaluate(self.sample())['accepted'])
    def test_reject_floor_only_impact(self):
        s=self.sample();s['case']='impact';s['contact_steps']=1000
        self.assertFalse(checks.evaluate(s)['accepted'])
    def test_reject_nan_metric(self):
        s=self.sample();s['max_penetration_m']=float('nan')
        self.assertFalse(checks.evaluate(s)['accepted'])
    def test_reject_incomplete_run(self):
        s=self.sample();s['completed']=False
        self.assertFalse(checks.evaluate(s)['accepted'])
    def test_reject_wrong_engine(self):
        s=self.sample();s['engine']='native replay'
        self.assertFalse(checks.evaluate(s)['accepted'])
    def test_contact_force_and_penetration_are_required(self):
        s=self.sample();s.update(case='contact',intended_contact_steps=20,max_penetration_m=.009,max_intended_contact_force_N=20)
        self.assertFalse(checks.evaluate(s)['accepted'])
        s['max_penetration_m']=.002
        self.assertTrue(checks.evaluate(s)['accepted'])
    def test_fault_must_disable_gate(self):
        s=self.sample();s.update(case='fault',final_fault=9,driver_fault_time_s=1,final_iq_A=0)
        self.assertFalse(checks.evaluate(s)['accepted'])
        s['final_gate_enabled']=False
        self.assertTrue(checks.evaluate(s)['accepted'])
    def test_unknown_case_is_rejected(self):
        s=self.sample();s['case']='whatever';self.assertFalse(checks.evaluate(s)['accepted'])
    def test_short_requested_duration_is_not_full_acceptance(self):
        s=self.sample();s.update(simulated_seconds=.08,expected_seconds=.08)
        self.assertFalse(checks.evaluate(s)['accepted'])
    def test_missing_provenance_is_rejected(self):
        s=self.sample();del s['mjcf_sha256'];self.assertFalse(checks.evaluate(s)['accepted'])
if __name__=='__main__':unittest.main()
