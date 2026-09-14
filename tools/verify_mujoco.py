#!/usr/bin/env python3
"""Run the pinned actual-engine acceptance lane. Never substitutes another engine.
Five scenarios plus four contact/impact substep refinements; recording optional.
Does not install dependencies, write GitHub state, flash hardware or loosen limits.
"""
from pathlib import Path
import argparse,importlib.util,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'results/mujoco_suite')
    p.add_argument('--library',type=Path);p.add_argument('--record',action='store_true')
    p.add_argument('--backend',choices=['egl','osmesa','glfw'],default='egl')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    report=dict(accepted=False,engine='MuJoCo',cases=[],analytical_fixture=None)
    dest=a.output/'summary.json'
    if importlib.util.find_spec('mujoco') is None:
        report.update(status='NOT_RUN',reason='mujoco==3.3.5 is not installed; no fallback used')
        dest.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 2
    sys.path.insert(0,str(ROOT/'examples/mujoco'))
    from runtime_checks import analytical_inertia_check
    report['analytical_fixture']=analytical_inertia_check()
    for case,sub in [(c,1) for c in ['tracking','disturbance','contact','impact','fault']]+[(c,n) for c in ['contact','impact'] for n in [2,4]]:
        out=a.output/f'{case}_sub{sub}'
        cmd=[sys.executable,str(ROOT/'examples/mujoco/run.py'),'--case',case,'--duration','2','--output',str(out),'--mechanical-substeps',str(sub),'--backend',a.backend]
        if a.library:cmd+=['--library',str(a.library.resolve())]
        if a.record and sub==1:cmd+=['--record']
        run=subprocess.run(cmd,capture_output=True,text=True)
        (a.output/f'{case}_sub{sub}.log').write_text(run.stdout+'\n'+run.stderr)
        item=json.loads((out/'summary.json').read_text()) if (out/'summary.json').exists() else dict(accepted=False,status='EXECUTION_FAILED')
        report['cases'].append(dict(case=case,mechanical_substeps=sub,exit_code=run.returncode,summary=item))
    refinement=[]
    for case in ['contact','impact']:
        for coarse,fine in [(1,2),(2,4)]:
            records={x['mechanical_substeps']:x['summary'] for x in report['cases'] if x['case']==case}
            b,c=records[coarse],records[fine];result=dict(case=case,coarse=coarse,fine=fine,accepted=False)
            if b.get('accepted') and c.get('accepted'):
                dq=abs(b['final_output_rad']-c['final_output_rad']);dp=abs(b['max_penetration_m']-c['max_penetration_m'])
                df=abs(b['max_intended_contact_force_N']-c['max_intended_contact_force_N'])/max(1.,c['max_intended_contact_force_N'])
                result.update(final_angle_difference_rad=dq,penetration_difference_m=dp,relative_peak_force_difference=df,accepted=dq<=.01 and dp<=.001 and df<=.20)
            refinement.append(result)
    report.update(status='EXECUTED',refinement=refinement,
        accepted=report['analytical_fixture']['accepted'] and all(x['exit_code']==0 and x['summary'].get('accepted') for x in report['cases']) and all(x['accepted'] for x in refinement),
        claim_boundary='Substep refinement holds electrical coupling at 50 us; not coupling-rate convergence, calibration, HIL or hardware acceptance.')
    dest.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');print(json.dumps(report,indent=2))
    return 0 if report['accepted'] else 4
if __name__=='__main__':sys.exit(main())
