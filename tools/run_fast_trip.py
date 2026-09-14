#!/usr/bin/env python3
"""Bounded synthetic OCP comparison, reusing qdd_sim (no second physics solver).
Exit 0 requires every selected case and available pair/convergence check to pass.
No hardware access; refuses an existing output folder.
"""
from __future__ import annotations
import argparse
import csv
import gzip
import json
import math
from pathlib import Path
import subprocess
import sys
from run_robustness import sha, source_sha, canonical_sha
ROOT=Path(__file__).resolve().parents[1]


def load_policy() -> dict:
    return json.loads((ROOT/'docs/FAST_TRIP_ACCEPTANCE.json').read_text())


def read_rows(path: Path) -> list[dict]:
    with path.open(newline='') as f:
        return [{k:float(row[k]) for k in ('time_s','interval_s','peak_A','interval_gate','endpoint_gate')} for row in csv.DictReader(f)]


def assess(r: dict, rows: list[dict], expected: str, policy: dict) -> dict:
    """Adjudicate exported events and actual gate intervals, not the process exit."""
    failure=[]
    def check(condition: bool, label: str) -> None:
        if not condition:failure.append(label)
    check(bool(rows),'missing_substep_trace')
    peak=r.get('max_abs_phase_current_A',float('nan'))
    final=r.get('final_iq_A',float('nan'))
    check(math.isfinite(peak) and math.isfinite(final),'nonfinite_result')
    events=r.get('protection_events',[])
    if expected=='nominal':
        check(r['fault']=='none' and r['state']=='armed','unexpected_fault_or_state')
        check(not events,'nominal_protection_event')
        return dict(status='FAIL' if failure else 'NOMINAL_NO_TRIP',reasons=failure,peak_A=peak)
    check(r['state']=='fault','missing_supervisor_fault')
    check(abs(final)<=policy['maximum_final_abs_iq_A'],'current_not_extinguished_locked_rotor')
    if expected=='software':
        check(r['fault']=='overcurrent','software_fault_reason')
        check(not r['fast_trip_enabled'] and not events,'software_only_contaminated')
        check(peak>=policy['minimum_software_stress_peak_A'],'stress_fixture_not_reproduced')
        crossing=next((x['time_s'] for x in rows if x['peak_A']>=45),None)
        return dict(status='FAIL' if failure else 'EXPECTED_SOFTWARE_TRIP',reasons=failure,
                    peak_A=peak,first_sample_at_or_above_45A_s=crossing,supervisor_s=r.get('first_fault_s'))
    check(r['fast_trip_enabled'],'fast_path_disabled')
    check(r['fault']=='gate_driver','fast_fault_reason')
    required=('current_crossing','comparator_assert','break_latch','pwm_inhibit','gates_off','supervisor_observed')
    selected={name:next((e for e in events if e['kind']==name),None) for name in required}
    for name,e in selected.items():check(e is not None,'missing_'+name)
    if any(e is None for e in selected.values()):return dict(status='FAIL',reasons=failure,peak_A=peak)
    cross,comp,latch,pwm,off,cpu=(selected[n] for n in required)
    for e in selected.values():
        check(e.get('current_valid',False),'invalid_model_current')
        check(math.isfinite(e['time_s']),'nonfinite_event_time')
    p=r['effective_config']['fast_trip'];tol=policy['timing_tolerance_s']
    lower,upper=cross['crossing_lower_s'],cross['crossing_upper_s']
    check(lower is not None and upper is not None,'missing_crossing_bracket')
    if lower is None or upper is None:return dict(status='FAIL',reasons=failure,peak_A=peak)
    check(math.isfinite(lower) and math.isfinite(upper) and 0<=upper-lower<=r['max_step_s']+tol,'crossing_bracket_resolution')
    check(abs(comp['time_s']-upper-p['propagation_s'])<=tol,'comparator_delay')
    check(abs(off['time_s']-latch['time_s']-p['gate_off_delay_s'])<=tol,'power_stage_delay')
    check(abs(pwm['time_s']-latch['time_s'])<=tol,'pwm_inhibit_not_at_break')
    if p['blanking_s']==0:
        check(abs(latch['time_s']-comp['time_s']-p['break_delay_s'])<=tol,'break_delay')
        check(peak<=policy['maximum_no_blanking_sampled_peak_A'],'peak_above_fixed_no_blanking_limit')
    else:
        check(latch['time_s']+tol>=comp['time_s']+p['break_delay_s'],'break_before_input')
    check(0<=cpu['time_s']-latch['time_s']<=policy['maximum_supervisor_delay_periods']/r['pwm_Hz']+tol,'supervisor_deadline')
    check(off['time_s']<cpu['time_s'],'not_off_before_supervisor_in_selected_fixture')
    check(off['current_peak_A']>0,'unphysical_instant_current_clamp')
    after=[x for x in rows if x['time_s']-x['interval_s']>=off['time_s']-tol]
    check(bool(after),'missing_post_off_intervals')
    check(all(x['interval_gate']==0 and x['endpoint_gate']==0 for x in after),'gate_reenabled_after_latch')
    return dict(status='FAIL' if failure else ('BLANKED_FAST_TRIP' if p['blanking_s'] else 'EXPECTED_FAST_TRIP'),
                reasons=failure,peak_A=peak,current_at_gate_off_A=off['current_peak_A'],
                crossing_lower_s=lower,crossing_upper_s=upper,comparator_s=comp['time_s'],
                break_s=latch['time_s'],gate_off_s=off['time_s'],supervisor_s=cpu['time_s'],
                crossing_to_gate_lower_s=off['time_s']-upper,crossing_to_gate_upper_s=off['time_s']-lower,
                gate_to_supervisor_s=cpu['time_s']-off['time_s'],
                blanking_extra_s=latch['time_s']-comp['time_s']-p['break_delay_s'])


def cases() -> list[dict]:
    result=[]
    for model,steps in [('averaged',[5,2.5,1.25]),('switched',[1,.5,.25])]:
        for h in steps:
            pair=model+'_'+str(h).replace('.','p')
            for expected in ['software','fast']:
                result.append(dict(id=pair+'_'+expected,model=model,step_us=h,expected=expected,pair=pair,
                                   overrides={'sensor.current_delay_cycles':8,'protection.enabled':int(expected=='fast')}))
    result += [dict(id='nominal_'+m,model=m,step_us=h,expected='nominal',overrides={'protection.enabled':1})
               for m,h in [('averaged',5),('switched',1)]]
    result += [dict(id='blanking_'+m,model=m,step_us=h,expected='fast',overrides={
        'sensor.current_delay_cycles':8,'protection.enabled':1,'protection.blanking_start_us':20,
        'protection.blanking_us':20}) for m,h in [('averaged',5),('switched',1)]]
    result.append(dict(id='conservative_delay8',model='averaged',step_us=5,expected='nominal',overrides={
        'sensor.current_delay_cycles':8,'drive.current_bandwidth_Hz':250,'protection.enabled':1}))
    result.append(dict(id='repeat_averaged_5_fast',model='averaged',step_us=5,expected='fast',overrides={
        'sensor.current_delay_cycles':8,'protection.enabled':1}))
    return result


def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--only',help='one exact case ID')
    args=ap.parse_args();exe=args.exe.resolve()
    if not exe.is_file():raise ValueError('Executable not found')
    selected=[c for c in cases() if args.only is None or c['id']==args.only]
    if not selected:raise ValueError('Unknown case ID')
    args.out.mkdir(parents=True,exist_ok=False)
    policy=load_policy();source=source_sha(ROOT)
    binding={'source_sha256':source,'executable_sha256':sha(exe),'runner_sha256':sha(Path(__file__)),
             'policy_sha256':sha(ROOT/'docs/FAST_TRIP_ACCEPTANCE.json'),
             'unchanged_legacy_policy_sha256':sha(ROOT/'docs/ACCEPTANCE.json')}
    (args.out/'policy.json').write_text(json.dumps(policy,indent=2)+'\n')
    (args.out/'cases.json').write_text(json.dumps(selected,indent=2)+'\n')
    records=[]
    for case in selected:
        folder=args.out/case['id'];folder.mkdir()
        profile=folder/'overrides.ini'
        profile.write_text('# Synthetic comparison. Not hardware OCP settings.\n'+''.join(f'{k}={v}\n' for k,v in sorted(case['overrides'].items())))
        cmd=[str(exe),'--case','locked-current','--model',case['model'],'--step-us',str(case['step_us']),
             '--duration','.08','--log-every','1','--protection-trace','--profile',str(profile.resolve()),'--output',str(folder.resolve())]
        proc=subprocess.run(cmd,text=True,capture_output=True,timeout=120)
        (folder/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');(folder/'stderr.txt').write_text(proc.stderr)
        stem='locked-current_'+case['model']+'_pi'
        if proc.returncode not in (0,3):raise RuntimeError(f"{case['id']}: simulator exit {proc.returncode}: {proc.stderr}")
        report=json.loads((folder/(stem+'.json')).read_text())
        if report['source_sha256']!=source:raise RuntimeError('Source/executable mismatch; rebuild')
        trace=folder/(stem+'_protection.csv');rows=read_rows(trace)
        assessment=assess(report,rows,case['expected'],policy)
        record=dict(case=case,metrics=assessment,report=report,process_exit_code=proc.returncode,
                    profile_sha256=sha(profile),trace_csv_sha256=sha(trace),config_sha256=canonical_sha(report['effective_config']))
        for path in folder.glob('*.csv'):
            path.with_suffix('.csv.gz').write_bytes(gzip.compress(path.read_bytes(),compresslevel=1,mtime=0));path.unlink()
        records.append(record);(folder/'assessment.json').write_text(json.dumps(record,indent=2,allow_nan=False)+'\n')
        print(case['id']+': '+assessment['status'],flush=True)
    pairs=[];convergence=[]
    for pair in sorted({r['case']['pair'] for r in records if 'pair' in r['case']}):
        group={r['case']['expected']:r for r in records if r['case'].get('pair')==pair}
        if set(group)!={'software','fast'}:continue
        sw,fast=group['software'],group['fast']
        def invariant(r):return {k:v for k,v in r['report']['effective_config'].items() if k!='fast_trip'}
        same=invariant(sw)==invariant(fast)
        improved=fast['report']['max_abs_phase_current_A']<sw['report']['max_abs_phase_current_A']
        pairs.append(dict(pair=pair,identical_non_protection_config=same,smaller_sampled_peak=improved,
                          status='PASS' if same and improved else 'FAIL'))
    for model in ['averaged','switched']:
        group=sorted([r for r in records if r['case'].get('pair') and r['case']['expected']=='fast' and r['case']['model']==model],
                     key=lambda r:r['case']['step_us'],reverse=True)
        for coarse,fine in zip(group,group[1:]):
            peak_delta=abs(coarse['metrics']['peak_A']-fine['metrics']['peak_A'])
            time_delta=abs(coarse['metrics']['gate_off_s']-fine['metrics']['gate_off_s'])
            passed=peak_delta<=policy['maximum_halving_peak_difference_A'] and time_delta<=policy['maximum_halving_gate_time_difference_periods']/coarse['report']['pwm_Hz']
            convergence.append(dict(model=model,coarse_us=coarse['case']['step_us'],fine_us=fine['case']['step_us'],
                                    peak_difference_A=peak_delta,gate_time_difference_s=time_delta,status='PASS' if passed else 'FAIL'))
    by_id={r['case']['id']:r for r in records}
    repeat=None
    if 'averaged_5_fast' in by_id and 'repeat_averaged_5_fast' in by_id:
        repeat=by_id['averaged_5_fast']['trace_csv_sha256']==by_id['repeat_averaged_5_fast']['trace_csv_sha256']
    passed=all(r['metrics']['status']!='FAIL' for r in records) and all(p['status']=='PASS' for p in pairs+convergence) and repeat is not False
    summary=dict(binding=binding,case_count=len(records),all_declared_checks_pass=passed,pairs=pairs,
                 timestep_halving=convergence,repeat_trace_identical=repeat,results=records,
                 hardware_validation='NOT_RUN',independent_circuit_solver='NOT_RUN',
                 claim='Bounded synthetic SIL event timing. No hardware safety or certification claim.')
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2,allow_nan=False)+'\n')
    with (args.out/'summary.csv').open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['case','status','sampled_peak_A','gate_off_s','supervisor_s','crossing_to_gate_min_us','crossing_to_gate_max_us'])
        for r in records:
            m=r['metrics'];writer.writerow([r['case']['id'],m['status'],m.get('peak_A'),m.get('gate_off_s'),m.get('supervisor_s'),
                                           m.get('crossing_to_gate_lower_s',0)*1e6,m.get('crossing_to_gate_upper_s',0)*1e6])
    return 0 if passed else 4

if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,RuntimeError,OSError,KeyError,subprocess.SubprocessError) as e:
        print('run_fast_trip: '+str(e),file=sys.stderr);sys.exit(2)
