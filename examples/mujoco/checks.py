"""Fixed, deliberately bounded SIL acceptance criteria. Not hardware limits.
Logic fixtures test these rules; only reports from actual MuJoCo runs can satisfy
the engine gate. Contact counts must be for the intended pair, not the floor.
"""
import math
import re

CASES=('tracking','disturbance','contact','impact','fault')
def evaluate(r):
    failures=[]
    def require(ok,message):
        if not ok:failures.append(message)
    require(r.get('engine')=='MuJoCo','engine must be MuJoCo, never a replay')
    require(r.get('version')=='3.3.5','pinned MuJoCo version 3.3.5 required')
    require(r.get('case') in CASES,'unknown scenario')
    require(r.get('completed') is True,'run incomplete')
    require(r.get('finite') is True,'non-finite state')
    require(r.get('warnings')==0,'engine emitted a numerical warning')
    for key in ('physics_trace_sha256','mjcf_sha256','library_sha256'):
        require(bool(re.fullmatch('[0-9a-f]{64}',str(r.get(key,'')))),f'missing {key}')
    fields=('simulated_seconds','expected_seconds','final_output_rad','target_rad','max_phase_current_A','intended_contact_steps','max_penetration_m','max_intended_contact_force_N','final_iq_A')
    for key in fields:
        require(isinstance(r.get(key),(float,int)) and math.isfinite(r[key]),f'invalid {key}')
    if failures:return dict(accepted=False,failures=failures)
    require(r['expected_seconds']>=1.6,'at least 1.6 s required for full scenario acceptance')
    require(abs(r['simulated_seconds']-r['expected_seconds'])<1e-8,'clock mismatch')
    require(r['max_phase_current_A']<60,'synthetic current acceptance ceiling exceeded')
    case=r['case']
    if case=='fault':
        require(r.get('final_fault')==9,'injected gate-driver fault not latched')
        require(r.get('final_gate_enabled') is False,'gates not disabled')
        t=r.get('driver_fault_time_s')
        require(isinstance(t,(int,float)) and math.isfinite(t) and 1.0<=t<=1.001,'fault detection outside expected window')
        require(abs(r['final_iq_A'])<.01,'fault current did not decay')
    else:
        require(r.get('final_fault')==0,'unexpected driver fault')
        if case in ('tracking','disturbance'):
            require(abs(r['final_output_rad']-r['target_rad'])<.1,'final target error exceeds 0.1 rad (impedance mode)')
            require(r['intended_contact_steps']==0,'unexpected intended contact')
        if case in ('contact','impact'):
            require(r['intended_contact_steps']>0,'required contact pair never touched')
            require(r['max_intended_contact_force_N']>0,'required pair generated no positive force')
            require(r['max_intended_contact_force_N']<500,'force exceeds bounded fixture acceptance ceiling')
            require(r['max_penetration_m']<(.008 if case=='contact' else .015),'penetration exceeds fixture bound')
    return dict(accepted=not failures,failures=failures)
