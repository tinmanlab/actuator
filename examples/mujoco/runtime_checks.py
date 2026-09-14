"""Actual-engine analytical fixture. Import/call only with MuJoCo installed.
This checks SI units, inertia ownership, axis and integration, not motor realism.
"""
def analytical_inertia_check():
    import mujoco
    h=50e-6;n=100;inertia=8e-5;torque=.012
    xml='''<mujoco><option timestep="0.00005" gravity="0 0 0" integrator="Euler"/>
    <worldbody><body><joint type="hinge" axis="0 1 0"/>
    <inertial pos="0 0 0" mass=".1" diaginertia=".00008 .00008 .00008"/>
    </body></worldbody></mujoco>'''
    model=mujoco.MjModel.from_xml_string(xml);data=mujoco.MjData(model)
    for _ in range(n):
        data.qfrc_applied[0]=torque;mujoco.mj_step(model,data)
    expected_velocity=torque/inertia*n*h
    expected_position=torque/inertia*h*h*n*(n+1)/2 # semi-implicit Euler discrete solution
    velocity_error=abs(float(data.qvel[0])-expected_velocity)
    position_error=abs(float(data.qpos[0])-expected_position)
    return dict(accepted=velocity_error<1e-10 and position_error<1e-10,
                expected_velocity_rad_s=expected_velocity,expected_position_rad=expected_position,
                velocity_error_rad_s=velocity_error,position_error_rad=position_error,
                engine='MuJoCo',version=mujoco.__version__,integration='semi-implicit Euler',steps=n,
                boundary='analytical mechanical fixture, not a calibrated electromechanical test')
