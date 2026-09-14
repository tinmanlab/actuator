"""Native executable + evidence tooling integration tests (no hardware)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from run_robustness import source_sha
from bench_analysis import read_trace,evaluate_step
parser=argparse.ArgumentParser();parser.add_argument('--exe',required=True);args,remaining=parser.parse_known_args()
EXE=str(Path(args.exe).resolve())
POLICY=json.loads((ROOT/'docs/ACCEPTANCE.json').read_text())

class WorkflowTests(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    def tearDown(self):self.tmp.cleanup()
    def run_case(self,name,extra=()):
        folder=self.root/name
        p=subprocess.run([EXE,'--case','locked-current','--duration','.12','--log-every','1','--output',str(folder),*extra],capture_output=True,text=True,timeout=30)
        self.assertIn(p.returncode,(0,3),p.stderr)
        return folder,json.loads(p.stdout)
    def test_source_binding_matches_compiled_input(self):
        _,r=self.run_case('binding')
        self.assertEqual(r['source_sha256'],source_sha(ROOT))
        self.assertNotEqual(r['source_sha256'],'0'*64)
    def test_seeded_trace_repeats_byte_for_byte(self):
        a,_=self.run_case('a');b,_=self.run_case('b')
        self.assertEqual((a/'locked-current_averaged_pi.csv').read_bytes(),(b/'locked-current_averaged_pi.csv').read_bytes())
    def test_process_success_is_not_performance_success(self):
        p=self.root/'delay.ini';p.write_text('sensor.current_delay_cycles=4\n')
        folder,r=self.run_case('delay',['--profile',str(p)])
        self.assertEqual(r['fault'],'none')
        m=evaluate_step(r,read_trace(folder/'locked-current_averaged_pi.csv'),POLICY['step'])
        self.assertEqual(m['status'],'PERFORMANCE_FAIL');self.assertIn('overshoot_pct',m['reasons'])
    def test_explicit_tuning_does_not_modify_plant(self):
        folder,r=self.run_case('tuned',['--profile',str(ROOT/'profiles/delay4_pi_conservative.ini')])
        m=evaluate_step(r,read_trace(folder/'locked-current_averaged_pi.csv'),POLICY['step'])
        self.assertEqual(m['status'],'PERFORMANCE_PASS')
        self.assertEqual(r['effective_config']['drive']['current_bandwidth_design_Hz'],300)
        self.assertEqual(r['effective_config']['plant_motor'],r['effective_config']['controller_motor'])
    def test_invalid_profile_exits_without_result(self):
        p=self.root/'invalid.ini';p.write_text('sensor.seed=4\nsensor.seed=5\n')
        run=subprocess.run([EXE,'--profile',str(p),'--output',str(self.root/'bad')],capture_output=True,text=True)
        self.assertEqual(run.returncode,2);self.assertFalse((self.root/'bad').exists())
    def test_cli_cannot_overwrite_prior_evidence(self):
        folder,_=self.run_case('retain')
        path=folder/'locked-current_averaged_pi.json'
        before=path.read_bytes()
        run=subprocess.run([EXE,'--case','locked-current','--duration','.12','--output',str(folder)],capture_output=True,text=True)
        self.assertEqual(run.returncode,2,run.stderr)
        self.assertIn('refusing to overwrite',run.stderr)
        self.assertEqual(path.read_bytes(),before)
    def test_study_cannot_overwrite_prior_evidence(self):
        out=self.root/'existing';out.mkdir();(out/'sentinel').write_text('keep')
        run=subprocess.run([sys.executable,str(ROOT/'tools/run_robustness.py'),'--exe',EXE,'--out',str(out),'--only','pi_nominal'],capture_output=True,text=True)
        self.assertEqual(run.returncode,2);self.assertEqual((out/'sentinel').read_text(),'keep')
    def test_require_all_flag_reports_performance_failure(self):
        run=subprocess.run([sys.executable,str(ROOT/'tools/run_robustness.py'),'--exe',EXE,'--out',str(self.root/'strict'),'--only','pi_delay_4','--require-all-performance'],capture_output=True,text=True,timeout=30)
        self.assertEqual(run.returncode,4,run.stderr)
        summary=json.loads((self.root/'strict/summary.json').read_text())
        self.assertTrue(summary['execution_complete']);self.assertEqual(summary['counts'],{'PERFORMANCE_FAIL':1})

if __name__=='__main__':unittest.main(argv=[sys.argv[0],*remaining])
