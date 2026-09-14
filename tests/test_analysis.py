"""Analytic signals test the offline metrics independently of the motor solver."""
import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from bench_analysis import step_metrics, sine_fit, bandwidth_bracket, evaluate_step

class AnalysisTests(unittest.TestCase):
    def trace(self, response):
        return [{'time_s': k*0.00005, 'iq_A': response(k*0.00005), 'iq_ref_A': 4 if k>=200 else 0}
                for k in range(1001)]
    def test_analytic_first_order_step(self):
        m=step_metrics(self.trace(lambda t: 4*(1-math.exp(-(t-.01)/.001)) if t>=.01 else 0))
        self.assertIn('settling_2pct_s',m)
        self.assertAlmostEqual(m['settling_2pct_s'], -math.log(.02)*.001, delta=.000051)
        self.assertAlmostEqual(m['rise_10_90_s'], math.log(9)*.001, delta=.000051)
        self.assertEqual(m['overshoot_pct'],0)
    def test_static_error_is_not_hidden_by_final_value(self):
        m=step_metrics(self.trace(lambda t: 3.8 if t>=.01 else 0))
        self.assertIn('settling_2pct_s',m)
        self.assertIsNone(m['settling_2pct_s'])
        self.assertAlmostEqual(m['tail_rmse_A'],.2)
    def test_end_spike_cannot_count_as_settled(self):
        rows=self.trace(lambda t: 4 if t>=.01 else 0);rows[-1]['iq_A']=4.5
        m=step_metrics(rows);self.assertIn('settling_2pct_s',m);self.assertIsNone(m['settling_2pct_s'])
    def test_empty_trace_rejected(self):
        with self.assertRaises(ValueError):step_metrics([])
    def test_sine_gain_phase_and_dc(self):
        f=127.3;phase=-.7
        t=[k*.00005 for k in range(2000)]
        m=sine_fit(t,[3+2*math.sin(2*math.pi*f*x+phase) for x in t],f)
        self.assertIn('amplitude',m);self.assertAlmostEqual(m['amplitude'],2,places=9)
        self.assertAlmostEqual(m['phase_rad'],phase,places=9);self.assertAlmostEqual(m['dc'],3,places=9)
        self.assertLess(m['residual_rms'],1e-6)
    def test_sine_insufficient_cycles_rejected(self):
        with self.assertRaises(ValueError):sine_fit([0,.001,.002],[0,1,0],10)
    def test_bandwidth_is_a_bracket_not_an_exact_measurement(self):
        m=bandwidth_bracket([{'frequency_Hz':10,'gain':1,'eligible':True},
                             {'frequency_Hz':500,'gain':.8,'eligible':True},
                             {'frequency_Hz':1000,'gain':.6,'eligible':True}])
        self.assertIn('bracket_Hz',m);self.assertEqual(m['bracket_Hz'],[500,1000])
    def test_no_crossing_is_censored_not_maximum_frequency(self):
        m=bandwidth_bracket([{'frequency_Hz':10,'gain':1,'eligible':True},
                             {'frequency_Hz':500,'gain':.9,'eligible':True}])
        self.assertIn('bracket_Hz',m);self.assertIsNone(m['bracket_Hz'])
        self.assertEqual(m['status'],'NO_CROSSING_IN_TESTED_RANGE')
    def test_ineligible_frequency_is_not_used_to_bridge_a_gap(self):
        m=bandwidth_bracket([{'frequency_Hz':10,'gain':1,'eligible':True},
                             {'frequency_Hz':500,'gain':None,'eligible':False},
                             {'frequency_Hz':1000,'gain':.6,'eligible':True}])
        self.assertIn('bracket_Hz',m);self.assertIsNone(m['bracket_Hz'])
    def test_coarse_trace_cannot_be_accepted(self):
        r={'effective_config':{'trace_divider':20}}
        m=evaluate_step(r,self.trace(lambda t: 4 if t>=.01 else 0),{})
        self.assertEqual(m.get('status'),'INSUFFICIENT_TRACE_RATE')

    def test_short_trace_is_insufficient_not_a_crash(self):
        r={'effective_config':{'trace_divider':1},'simulated_seconds':.005}
        result=evaluate_step(r,[{'time_s':.001,'iq_A':0}],{'step_s':.01,'minimum_horizon_s':.04})
        self.assertEqual(result['status'],'INSUFFICIENT_HORIZON')
    def test_frequency_response_reports_resonance(self):
        result=bandwidth_bracket([{'frequency_Hz':10,'gain':1,'eligible':True},
                                  {'frequency_Hz':500,'gain':2,'eligible':True},
                                  {'frequency_Hz':1000,'gain':.6,'eligible':True}])
        self.assertIn('peak_gain_relative_dB',result)
        self.assertAlmostEqual(result['peak_gain_relative_dB'],20*math.log10(2))

if __name__=='__main__':unittest.main()
