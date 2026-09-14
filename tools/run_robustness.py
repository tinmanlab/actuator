#!/usr/bin/env python3
"""Run the native C++ benchmark; classify performance separately from process exit codes.
No hardware access. Output directory must be new to avoid destroying prior evidence.
"""
from __future__ import annotations
import argparse
import csv
import gzip
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
from bench_analysis import read_trace, evaluate_step, evaluate_frequency, bandwidth_bracket
ROOT = Path(__file__).resolve().parents[1]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_sha(value: object) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()).hexdigest()


def source_sha(root: Path) -> str:
    paths = [root/'CMakeLists.txt']
    for folder, suffixes in [('src',{'.cpp'}),('include',{'.hpp','.h'}),('apps',{'.cpp'}),('cmake',{'.cmake'})]:
        paths += [p for p in (root/folder).rglob('*') if p.is_file() and p.suffix in suffixes]
    lines = ''.join(f'{p.relative_to(root).as_posix()}:{sha(p)}\n' for p in sorted(paths,key=lambda p:p.relative_to(root).as_posix()))
    return hashlib.sha256(lines.encode()).hexdigest()


def assess(case: dict, report: dict, rows: list[dict], policy: dict) -> dict:
    kind=case['assessment']
    if kind=='step': return evaluate_step(report,rows,policy['step'])
    if kind=='frequency': return evaluate_frequency(report,rows,policy['frequency'])
    if kind=='protection':
        expected={'gate-fault':'gate_driver','watchdog':'command_timeout','sensor-fault':'sensor','overvoltage':'overvoltage'}[case['scenario']]
        latency=report['fault_latency_s']
        bound=policy['protection']['maximum_injected_fault_latency_control_periods']/report['pwm_Hz']
        passed=report['fault']==expected and report['state']=='fault' and latency is not None and 0<=latency<=bound+1e-12
        return {'status':'EXPECTED_PROTECTION_PASS' if passed else 'PROTECTION_FAIL','expected_fault':expected,'observed_fault':report['fault'],'latency_s':latency}
    if report['fault']!='none':return {'status':'PROTECTION_TRIP','reasons':[report['fault']]}
    if kind=='torque':
        p=policy['torque'];tail=[r['gear_torque_Nm'] for r in rows if r['time_s']>=report['simulated_seconds']*.8]
        error=math.sqrt(sum((v-p['target_Nm'])**2 for v in tail)/len(tail))
        enough=report['simulated_seconds']>=p['minimum_duration_s']
        return {'status':('PERFORMANCE_PASS' if error<=p['maximum_tail_rmse_Nm'] else 'PERFORMANCE_FAIL') if enough else 'INSUFFICIENT_HORIZON',
                'tail_torque_mean_Nm':sum(tail)/len(tail),'tail_torque_rmse_Nm':error,'target_Nm':p['target_Nm']}
    if kind=='impedance':
        p=policy['impedance'];error=abs(report['final_position_rad']-p['equilibrium_rad'])
        passed=error<=p['maximum_final_error_rad'] and abs(report['final_speed_rad_s'])<=p['maximum_final_speed_rad_s']
        return {'status':('PERFORMANCE_PASS' if passed else 'PERFORMANCE_FAIL') if report['simulated_seconds']>=p['minimum_duration_s']-1e-9 else 'INSUFFICIENT_HORIZON',
                'final_equilibrium_error_rad':error,'expected_equilibrium_rad':p['equilibrium_rad']}
    raise ValueError('Unknown assessment type: '+kind)


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--cases',type=Path,default=ROOT/'profiles/robustness_cases.json')
    parser.add_argument('--policy',type=Path,default=ROOT/'docs/ACCEPTANCE.json')
    parser.add_argument('--only',help='Select one exact case ID')
    parser.add_argument('--require-all-performance',action='store_true',help='Exit 4 if any case fails its performance/FRF/protection criteria')
    args=parser.parse_args();exe=args.exe.resolve()
    suite=json.loads(args.cases.read_text());policy=json.loads(args.policy.read_text())
    cases=[c for c in suite['cases'] if args.only is None or c['id']==args.only]
    if not cases or len({c['id'] for c in cases})!=len(cases):raise ValueError('No selected cases or duplicate IDs')
    if any(not re.fullmatch(r'[A-Za-z0-9_-]+',c['id']) for c in cases):raise ValueError('Invalid case ID')
    args.out.mkdir(parents=True,exist_ok=False)
    expected_source=source_sha(ROOT)
    binding={'source_sha256':expected_source,'executable_sha256':sha(exe),'suite_sha256':sha(args.cases),
             'policy_sha256':sha(args.policy),'analysis_sha256':sha(ROOT/'tools/bench_analysis.py'),
             'runner_sha256':sha(Path(__file__))}
    # Preserve exact study inputs with the outputs.
    (args.out/'suite.json').write_text(json.dumps(suite,indent=2)+'\n')
    (args.out/'policy.json').write_text(json.dumps(policy,indent=2)+'\n')
    records=[];groups={};execution_errors=[]
    for case in cases:
        folder=args.out/case['id'];folder.mkdir()
        profile=folder/'overrides.ini'
        profile.write_text('# Synthetic robustness fixture; controller remains nominal unless explicitly overridden.\n'+
                           ''.join(f'{k}={v}\n' for k,v in sorted(case['overrides'].items())))
        cmd=[str(exe),'--case',case['scenario'],'--algorithm',case['algorithm'],'--duration',str(case['duration']),
             '--log-every','1','--profile',str(profile.resolve()),'--output',str(folder.resolve())]
        for key, flag in [('model','--model'),('step_us','--step-us'),('frequency_hz','--frequency-hz')]:
            if key in case:cmd.extend([flag,str(case[key])])
        # Numerical resolution must be set before profile validation for high-R fixtures.
        if 'step_us' in case:
            at=cmd.index('--step-us');pair=cmd[at:at+2];del cmd[at:at+2];cmd[1:1]=pair
        process=subprocess.run(cmd,capture_output=True,text=True,timeout=120,check=False)
        (folder/'stderr.txt').write_text(process.stderr)
        (folder/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
        stem=f"{case['scenario']}_{case.get('model','averaged')}_{case['algorithm']}"
        record={'id':case['id'],'assessment':case['assessment'],'process_exit_code':process.returncode,'profile_sha256':sha(profile)}
        report_path=folder/(stem+'.json')
        if process.returncode not in (0,3) or not report_path.exists():
            record.update(status='EXECUTION_ERROR',stderr=process.stderr);execution_errors.append(case['id'])
        else:
            report=json.loads(report_path.read_text())
            if report['source_sha256']!=expected_source:raise RuntimeError('Executable does not match source tree; rebuild before running study')
            trace=folder/(stem+'.csv');rows=read_trace(trace)
            record.update(report=report,metrics=assess(case,report,rows,policy),
                          config_sha256=canonical_sha(report['effective_config']),
                          seed=report['effective_config']['sensor']['seed'],trace_csv_sha256=sha(trace))
            record['status']=record['metrics']['status']
            if 'group' in case:groups.setdefault(case['group'],[]).append(record['metrics'])
            # Gzip preserves every row; mtime=0 makes archive bytes reproducible.
            for path in folder.glob('*.csv'):
                zipped=path.with_suffix(path.suffix+'.gz');zipped.write_bytes(gzip.compress(path.read_bytes(),mtime=0));path.unlink()
            record['trace_gzip_sha256']=sha(folder/(stem+'.csv.gz'))
        (folder/'assessment.json').write_text(json.dumps(record,indent=2,allow_nan=False)+'\n')
        records.append(record);print(case['id']+': '+record['status'],flush=True)
    counts={s:sum(r['status']==s for r in records) for s in sorted({r['status'] for r in records})}
    summary={'binding':binding,'execution_complete':not execution_errors,'case_count':len(records),'counts':counts,
             'bandwidth':{k:bandwidth_bracket(v) for k,v in groups.items()},'results':records,
             'hardware_correlation':'NOT_RUN','independent_circuit_solver_comparison':'NOT_RUN',
             'claim':'Finite-horizon synthetic SIL evidence; stress-case performance failures are retained, not waived.'}
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2,allow_nan=False)+'\n')
    with (args.out/'summary.csv').open('w',newline='') as f:
        fields=['id','status','process_exit_code','fault','settling_s','overshoot_pct','tail_rmse_A','max_phase_A','saturation_fraction']
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
        for r in records:
            m=r.get('metrics',{});v=r.get('report',{})
            writer.writerow(dict(id=r['id'],status=r['status'],process_exit_code=r['process_exit_code'],fault=v.get('fault'),
                                 settling_s=m.get('settling_2pct_s'),overshoot_pct=m.get('overshoot_pct'),tail_rmse_A=m.get('tail_rmse_A'),
                                 max_phase_A=v.get('max_abs_phase_current_A'),saturation_fraction=v.get('voltage_saturation_fraction')))
    if execution_errors:return 2
    accepted={'PERFORMANCE_PASS','EXPECTED_PROTECTION_PASS','ELIGIBLE_SMALL_SIGNAL_WINDOW'}
    return 4 if args.require_all_performance and any(r['status'] not in accepted for r in records) else 0

if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as exc:
        print(f'run_robustness: {exc}',file=sys.stderr);sys.exit(2)
