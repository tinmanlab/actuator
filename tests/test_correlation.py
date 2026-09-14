import csv
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from correlate import load_measurement, compare_measurement

class CorrelationTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.meta={'schema':1,'origin':'synthetic','run_id':'unit-test','provenance':'analytic unit-test fixture',
                   'current_convention':'amplitude_invariant_dq_peak','current_frame':'rotor_electrical',
                   'current_unit':'A','time_unit':'s','position_unit':'rad','encoder_side':'output',
                   'expected_sample_period_s':.001,'time_shift_s':0,
                   'columns':{'time':'t','id':'id','iq':'iq','position':'q','bus':'bus'},'bus_unit':'V'}
        self.rows=[[k*.001,0,1+k,0.1*k,48] for k in range(5)]
    def tearDown(self):self.tmp.cleanup()
    def paths(self):
        p=self.root/'m.csv';m=self.root/'m.json'
        with p.open('w',newline='') as f:
            w=csv.writer(f);w.writerow(['t','id','iq','q','bus']);w.writerows(self.rows)
        m.write_text(json.dumps(self.meta));return p,m
    def load(self):return load_measurement(*self.paths())
    def test_milli_units_and_degrees(self):
        self.meta.update(time_unit='ms',current_unit='mA',position_unit='deg')
        self.rows=[[k,0,1000*(k+1),math.degrees(.1*k),48] for k in range(5)]
        data,_=self.load();self.assertTrue(data)
        self.assertAlmostEqual(data[2]['time_s'],.002);self.assertAlmostEqual(data[2]['iq_A'],3)
        self.assertAlmostEqual(data[2]['output_position_rad'],.2)
    def test_rms_is_not_silently_phase_peak(self):
        self.meta['current_convention']='phase_rms'
        with self.assertRaises(ValueError):self.load()
    def test_unknown_time_unit_rejected(self):
        self.meta['time_unit']='ticks'
        with self.assertRaises(ValueError):self.load()
    def test_motor_encoder_is_not_an_output_encoder(self):
        self.meta['encoder_side']='motor'
        with self.assertRaises(ValueError):self.load()
    def test_duplicate_time_rejected(self):
        self.rows[2][0]=self.rows[1][0]
        with self.assertRaises(ValueError):self.load()
    def test_missing_sample_rejected(self):
        del self.rows[2]
        with self.assertRaises(ValueError):self.load()
    def test_nan_rejected(self):
        self.rows[2][2]=float('nan')
        with self.assertRaises(ValueError):self.load()
    def test_missing_manifest_field_rejected(self):
        del self.meta['current_frame']
        with self.assertRaises(ValueError):self.load()
    def test_duplicate_column_mapping_rejected(self):
        self.meta['columns']['id']='iq'
        with self.assertRaises(ValueError):self.load()
    def test_known_error_and_no_hardware_promotion(self):
        data,meta=self.load();ref=[dict(row,iq_A=row['iq_A']-.25) for row in data]
        result=compare_measurement(ref,data,meta)
        self.assertIn('signals',result);self.assertAlmostEqual(result['signals']['iq_A']['rmse'],.25)
        self.assertEqual(result['hardware_validation'],'NOT_RUN')
    def test_no_extrapolation(self):
        data,meta=self.load();ref=[dict(r,time_s=r['time_s']+1) for r in data]
        with self.assertRaises(ValueError):compare_measurement(ref,data,meta)
    def test_encoder_counts_require_resolution(self):
        self.meta['position_unit']='counts'
        with self.assertRaises(ValueError):self.load()
    def test_encoder_counts_are_explicitly_converted(self):
        self.meta.update(position_unit='counts',encoder_counts_per_turn=4096)
        self.rows[2][3]=1024
        data,_=self.load();self.assertTrue(data);self.assertAlmostEqual(data[2]['output_position_rad'],math.pi/2)
    def test_time_shift_requires_reason(self):
        self.meta['time_shift_s']=.1
        with self.assertRaises(ValueError):self.load()
    def test_claimed_measurement_requires_hardware_identity(self):
        self.meta['origin']='user_declared_measurement'
        with self.assertRaises(ValueError):self.load()
if __name__=='__main__':unittest.main()
