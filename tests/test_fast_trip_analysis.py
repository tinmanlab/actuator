"""Analytic checks of the fast-trip evidence adjudicator, not a plant model."""
import copy
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import run_fast_trip as analysis

def fixture():
    events=[]
    for kind,t in [('current_crossing',.001),('comparator_assert',.0010002),('break_latch',.0010003),
                   ('pwm_inhibit',.0010003),('gates_off',.0010005),('supervisor_observed',.00105)]:
        events.append(dict(kind=kind,time_s=t,current_peak_A=45.4,crossing_lower_s=.000999,
                           crossing_upper_s=.001,current_valid=True,blanked=False,episode=1))
    r=dict(fault='gate_driver',state='fault',fast_trip_enabled=True,source_sha256='fixture',
           max_abs_phase_current_A=45.4,final_iq_A=0,max_step_s=1e-6,pwm_Hz=20000,
           protection_events=events,effective_config={'fast_trip':dict(enabled=True,threshold_A=45,
           propagation_s=2e-7,break_delay_s=1e-7,gate_off_delay_s=2e-7,blanking_s=0)})
    rows=[dict(time_s=.0010005,interval_s=2e-7,peak_A=45.4,interval_gate=1,endpoint_gate=0),
          dict(time_s=.001002,interval_s=1.5e-6,peak_A=44.0,interval_gate=0,endpoint_gate=0)]
    return r,rows
class FastTripAnalysisTests(unittest.TestCase):
    def test_known_latency_bracket(self):
        r,rows=fixture();m=analysis.assess(r,rows,'fast',analysis.load_policy())
        self.assertEqual(m['status'],'EXPECTED_FAST_TRIP')
        self.assertAlmostEqual(m['crossing_to_gate_lower_s'],.5e-6)
        self.assertAlmostEqual(m['crossing_to_gate_upper_s'],1.5e-6)
    def test_missing_event_fails(self):
        r,rows=fixture();r['protection_events']=r['protection_events'][:-1]
        self.assertEqual(analysis.assess(r,rows,'fast',analysis.load_policy())['status'],'FAIL')
    def test_reenabled_gate_fails(self):
        r,rows=fixture();rows[-1]['interval_gate']=1
        self.assertEqual(analysis.assess(r,rows,'fast',analysis.load_policy())['status'],'FAIL')
    def test_late_supervisor_fails(self):
        r,rows=fixture();r['protection_events'][-1]['time_s']+=.001
        self.assertEqual(analysis.assess(r,rows,'fast',analysis.load_policy())['status'],'FAIL')
    def test_oversized_crossing_bracket_fails(self):
        r,rows=fixture();r['protection_events'][0]['crossing_lower_s']=0
        self.assertEqual(analysis.assess(r,rows,'fast',analysis.load_policy())['status'],'FAIL')
    def test_no_trace_is_not_proof(self):
        r,_=fixture();self.assertEqual(analysis.assess(r,[],'fast',analysis.load_policy())['status'],'FAIL')
    def test_nonfinite_peak_rejected(self):
        r,rows=fixture();r['max_abs_phase_current_A']=float('nan')
        self.assertEqual(analysis.assess(r,rows,'fast',analysis.load_policy())['status'],'FAIL')
if __name__=='__main__':unittest.main()
