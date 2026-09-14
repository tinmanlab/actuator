#!/usr/bin/env python3
"""Actual MuJoCo + native C++ co-simulation. Never substitutes replay physics.
Exit 0: bounded scenario accepted; 2: dependency not run; 4: executed, not accepted.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,math,os,shutil,subprocess,sys,time
from pathlib import Path
from bridge import Drive,Input
from checks import CASES,evaluate

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--case',choices=CASES,default='tracking')
    p.add_argument('--duration',type=float,default=2.)
    p.add_argument('--output',type=Path,default=Path('results/mujoco'))
    p.add_argument('--library',type=Path)
    p.add_argument('--live',action='store_true');p.add_argument('--record',action='store_true')
    p.add_argument('--switched',action='store_true');p.add_argument('--predictive',action='store_true')
    p.add_argument('--backend',choices=['egl','osmesa','glfw'])
    p.add_argument('--mechanical-substeps',type=int,choices=[1,2,4,8],default=1,
                   help='refines mechanics/contact only; the C++ boundary remains 50 us')
    p.add_argument('--playback-speed',type=float,default=.25)
    a=p.parse_args()
    if not math.isfinite(a.duration) or not 0<a.duration<=60:p.error('duration must be in (0,60]')
    if not math.isfinite(a.playback_speed) or not .1<=a.playback_speed<=4:p.error('playback speed must be in [.1,4]')
    if a.backend:os.environ['MUJOCO_GL']=a.backend
    a.output.mkdir(parents=True,exist_ok=False)
    try:
        import mujoco
        import numpy as np
    except ImportError as exc:
        report=dict(engine='MuJoCo',status='NOT_RUN',accepted=False,reason=str(exc),case=a.case)
        (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2));return 2
    if a.record and not shutil.which('ffmpeg'):raise RuntimeError('Recording requires ffmpeg on PATH')
    xml=Path(__file__).with_name('bench.xml')
    model=mujoco.MjModel.from_xml_path(str(xml));data=mujoco.MjData(model)
    model.opt.enableflags |= int(mujoco.mjtEnableBit.mjENBL_ENERGY)
    def ident(kind,name):
        i=mujoco.mj_name2id(model,kind,name)
        if i<0:raise ValueError('Model is missing '+name)
        return i
    rotor=ident(mujoco.mjtObj.mjOBJ_JOINT,'rotor_joint');output=ident(mujoco.mjtObj.mjOBJ_JOINT,'output_joint')
    rq,rd=int(model.jnt_qposadr[rotor]),int(model.jnt_dofadr[rotor])
    oq,od=int(model.jnt_qposadr[output]),int(model.jnt_dofadr[output])
    # Hide inactive scenario parts, as well as disabling their collision masks.
    for name,enabled in [('stopper',a.case=='contact'),('stop_foot',a.case=='contact'),('stop_column',a.case=='contact'),('ball',a.case=='impact')]:
        g=ident(mujoco.mjtObj.mjOBJ_GEOM,name)
        if not enabled:
            model.geom_contype[g]=model.geom_conaffinity[g]=0;model.geom_rgba[g,3]=0
    if a.case!='impact':model.body_gravcomp[ident(mujoco.mjtObj.mjOBJ_BODY,'impact_ball')]=1
    mujoco.mj_forward(model,data)
    initial_energy=float(sum(data.energy))
    state=dict(pause=False,fault=False,pulse_until=-1.,target=.85)
    def key(k):
        if k==32:state['pause']=not state['pause']
        elif k in (ord('D'),ord('d')):state['pulse_until']=float(data.time)+.12
        elif k in (ord('F'),ord('f')):state['fault']=True
        elif k in (ord('+'),ord('=')):state['target']=min(1.1,state['target']+.1)
        elif k==ord('-'):state['target']=max(-.5,state['target']-.1)
    viewer=renderer=encoder=None
    count=steps=frames=contact_steps=intended_steps=0;max_force=max_pen=max_current=0.
    fault_time=None;pair_counts={};finite=True;work_drive=work_disturbance=0.
    report={};start=time.perf_counter()
    fields=['time_s','output_rad','rotor_rad','output_rad_s','iq_A','iq_ref_A','output_torque_Nm','vbus_V','state','fault','gate_enabled','contacts','intended_contact_force_N','intended_penetration_m','disturbance_Nm','mechanical_energy_J','drive_work_J','disturbance_work_J']
    try:
        with Drive(a.library,switched=a.switched,predictive=a.predictive) as drive:
            if abs(float(model.opt.timestep)-drive.period)>1e-12:raise RuntimeError('Expected original model timestep to equal 50 us coupling period')
            model.opt.timestep=drive.period/a.mechanical_substeps
            if a.live:
                import mujoco.viewer
                viewer=mujoco.viewer.launch_passive(model,data,key_callback=key)
                viewer.cam.lookat[:]=[.025,.015,.44];viewer.cam.distance=1.25;viewer.cam.azimuth=120;viewer.cam.elevation=-20
            if a.record:
                from PIL import Image,ImageDraw,ImageFont
                renderer=mujoco.Renderer(model,height=720,width=1280)
                camera=mujoco.MjvCamera();camera.lookat[:]=[.025,.015,.44];camera.distance=1.25;camera.azimuth=120;camera.elevation=-20
                try:font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',19)
                except OSError:font=ImageFont.load_default()
                encoder=subprocess.Popen(['ffmpeg','-v','error','-f','rawvideo','-pixel_format','rgb24','-video_size','1280x720','-framerate','30','-i','-','-an','-c:v','libx264','-crf','22','-pix_fmt','yuv420p','-movflags','+faststart',str(a.output/'mujoco.mp4')],stdin=subprocess.PIPE)
            trace=a.output/'trace.csv'
            with trace.open('w',newline='') as stream:
                writer=csv.writer(stream);writer.writerow(fields)
                ticks=round(a.duration/drive.period)
                for tick in range(ticks):
                    if viewer and not viewer.is_running():break
                    while viewer and state['pause'] and viewer.is_running():viewer.sync();time.sleep(.02)
                    t=tick*drive.period
                    if abs(float(data.time)-t)>1e-8:raise RuntimeError('Coupling clock mismatch')
                    pulse=-5. if ((a.case=='disturbance' and .7<=t<.82) or t<state['pulse_until']) else 0.
                    inp=Input(float(data.qpos[rq]),float(data.qvel[rd]),float(data.qpos[oq]),float(data.qvel[od]),state['target'] if t>=.1 else 0.,0,0,0,25,1.8,4,1,int(state['fault'] or (a.case=='fault' and t>=1.)))
                    out=drive.tick(inp);max_current=max(max_current,out.phase_peak)
                    if out.fault and fault_time is None:fault_time=t
                    data.qfrc_applied[rd]=out.rotor_torque;data.qfrc_applied[od]=out.output_torque+pulse
                    cf=pen=0.
                    for _ in range(a.mechanical_substeps):
                        r0,o0=float(data.qpos[rq]),float(data.qpos[oq])
                        mujoco.mj_step(model,data);steps+=1
                        # Recompute at the endpoint; mj_step's contact arrays otherwise
                        # refer to the beginning of the integration step.
                        mujoco.mj_forward(model,data)
                        work_drive+=out.rotor_torque*(data.qpos[rq]-r0)+out.output_torque*(data.qpos[oq]-o0)
                        work_disturbance+=pulse*(data.qpos[oq]-o0)
                        contact_steps+=int(data.ncon>0);this_intended=False
                        for j in range(data.ncon):
                            c=data.contact[j]
                            names=[mujoco.mj_id2name(model,mujoco.mjtObj.mjOBJ_GEOM,int(g)) for g in (c.geom1,c.geom2)]
                            pair='|'.join(sorted(names));pair_counts[pair]=pair_counts.get(pair,0)+1
                            intended=(('stopper' in names and any(n in names for n in ('link','tip'))) if a.case=='contact' else ('ball' in names and any(n in names for n in ('link','tip'))) if a.case=='impact' else False)
                            if intended:
                                f=np.zeros(6);mujoco.mj_contactForce(model,data,j,f)
                                cf=max(cf,float(np.linalg.norm(f[:3])));pen=max(pen,max(0.,-float(c.dist)));this_intended=True
                        intended_steps+=int(this_intended)
                    max_force=max(max_force,cf);max_pen=max(max_pen,pen);count+=1
                    finite=finite and bool(np.isfinite(data.qpos).all() and np.isfinite(data.qvel).all() and all(math.isfinite(v) for v in (out.iq,out.vbus,out.rotor_torque,out.output_torque,work_drive)))
                    if not finite:raise RuntimeError('Non-finite co-simulation state')
                    writer.writerow([data.time,data.qpos[oq],data.qpos[rq],data.qvel[od],out.iq,out.iq_ref,out.output_torque,out.vbus,out.state,out.fault,out.gate_enabled,data.ncon,cf,pen,pulse,sum(data.energy),work_drive,work_disturbance])
                    if renderer and data.time+1e-12 >= frames/30*a.playback_speed:
                        renderer.update_scene(data,camera=camera);im=Image.fromarray(renderer.render());d=ImageDraw.Draw(im)
                        d.rectangle((0,0,1280,95),fill=(16,25,35))
                        d.text((24,12),'ACTUATOR LAB | Actual MuJoCo + C++ FOC | '+a.case.upper(),font=font,fill='white')
                        d.text((24,42),f't={data.time:.3f}s   q={data.qpos[oq]:.3f}rad   iq={out.iq:.2f}A   torque={out.output_torque:.2f}Nm   gate={out.gate_enabled}   fault={out.fault}',font=font,fill='white')
                        d.text((24,68),'Illustrative board and motor | Lumped reducer | Synthetic parameters | No hardware validation',font=font,fill=(183,204,221))
                        encoder.stdin.write(im.tobytes());frames+=1
                    if viewer and tick%200==0:
                        viewer.sync();time.sleep(max(0,float(data.time)-(time.perf_counter()-start)))
            if encoder:
                encoder.stdin.close()
                result=encoder.wait();encoder=None
                if result!=0:raise RuntimeError('Video encoder failed')
            report=dict(engine='MuJoCo',version=mujoco.__version__,status='EXECUTED',case=a.case,
                completed=count==ticks,finite=finite,warnings=int(sum(w.number for w in data.warning)),
                coupling_period_s=drive.period,mechanical_timestep_s=float(model.opt.timestep),
                mechanical_steps=steps,control_steps=count,simulated_seconds=float(data.time),expected_seconds=ticks*drive.period,
                wall_seconds=time.perf_counter()-start,contact_steps=contact_steps,intended_contact_steps=intended_steps,
                contact_pairs=pair_counts,max_intended_contact_force_N=max_force,max_penetration_m=max_pen,
                max_phase_current_A=max_current,final_output_rad=float(data.qpos[oq]),target_rad=state['target'],
                final_iq_A=out.iq,final_fault=out.fault,final_gate_enabled=bool(out.gate_enabled),driver_fault_time_s=fault_time,
                initial_mechanical_energy_J=initial_energy,final_mechanical_energy_J=float(sum(data.energy)),
                drive_work_J=float(work_drive),disturbance_work_J=float(work_disturbance),
                energy_residual_includes_passive_and_contact_losses_J=float(sum(data.energy)-initial_energy-work_drive-work_disturbance),
                mjcf_sha256=sha(xml),mesh_sha256={m.name:sha(m) for m in sorted(xml.with_name('assets').glob('*.obj'))},
                library_sha256=sha(drive.path),physics_trace_sha256=sha(trace),video_frames=frames,
                hardware_validation='NOT_RUN',coupling='explicit 50 us ZOH torques; mechanics substeps do not refine coupling',
                electrical_equations='native C++',rendered_geometry='illustrative; collider proxies and inertial sources preserved')
            report.update(evaluate(report))
            (a.output/'summary.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
            print(json.dumps(report,indent=2,allow_nan=False))
            return 0 if report['accepted'] else 4
    except Exception as exc:
        report=dict(engine='MuJoCo',status='EXECUTION_FAILED',accepted=False,case=a.case,
                    reason=f'{type(exc).__name__}: {exc}',control_steps=count,mechanical_steps=steps)
        (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2),file=sys.stderr)
        return 4
    finally:
        if viewer:viewer.close()
        if renderer:renderer.close()
        if encoder:
            encoder.stdin.close()
            if encoder.wait()!=0:raise RuntimeError('Video encoder failed; recording not accepted')
if __name__=='__main__':sys.exit(main())
