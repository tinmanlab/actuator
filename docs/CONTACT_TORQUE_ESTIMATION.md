# Joint-torque contact estimation research lane

This lane uses the repository's existing electromechanical drive and MuJoCo
co-simulation boundary to study sensorless contact estimation without replacing
the actuator with an ideal torque source.

## Scope

The native QDD model owns FOC, current sensing and delay, inverter behavior,
PMSM torque, reducer compliance/backlash and transmitted output torque. MuJoCo
owns rotor/link inertia, gravity, passive joint friction and contact. The same
mechanical degree of freedom is never integrated twice.

The first target is deliberately one degree of freedom. It answers two separate
questions before moving to a floating-base robot:

1. Can the link-side inertial model be identified from known free-space motion?
2. After subtracting that model, can proprioceptive joint torque reveal an
   external contact generalized torque?

A Cartesian contact force is **not** generally observable from one joint torque.
The primary quantity is

\[
\tau_\mathrm{ext} = J_c(q)^\top f_c.
\]

A scalar normal force is only reported when the contact point and normal are
known and the scalar normal Jacobian is non-singular.

## Dynamics and identification

For the simple hinge fixture the experiment fits the lumped model

\[
I\ddot q
+ g_s\sin q
+ g_c\cos q
+ b\dot q
+ f_c\,\mathrm{sign}(\dot q)
+ \tau_0
= \tau_a + \tau_\mathrm{ext}.
\]

The contact-free excitation lane estimates
\(\phi=[I,g_s,g_c,b,f_c,\tau_0]\) by linear least squares. The MuJoCo mass
matrix is retained only as independent truth for the identified inertia.

This is intentionally smaller than the whole-body ICRA 2025 method by
Khorshidi et al., which writes rigid-body dynamics as

\[
Y(q,v,\dot v)\phi=S^\top\tau+J_c^\top\lambda
\]

and projects them with
\(P=I-J_c^\dagger J_c\):

\[
P\,Y(q,v,\dot v)\phi=P\,S^\top\tau.
\]

That contact-nullspace projection removes the unknown contact force and is the
correct next step for multi-link/floating-base identification. The present
one-link lane uses a contact-free identification phase instead of pretending
that inertia and an arbitrary unknown contact torque are separately identifiable
from the same scalar equation.

Reference and code:
- Khorshidi et al., *Physically-Consistent Parameter Identification of Robots
  in Contact*, ICRA 2025 / arXiv:2409.09850.
- https://github.com/ShahramKhorshidi/system_identification

## MIT discrete generalized-momentum observer

Bledt et al. start from

\[
M(q)\ddot q+C(q,\dot q)\dot q+g(q)
= S^\top\tau+\tau_d
\]

and derive the disturbance observer directly in discrete time. With

\[
\gamma=e^{-\lambda\Delta t},\qquad
\beta=\frac{1-\gamma}{\gamma\Delta t},
\]

their Eq. (10) is

\[
\hat\tau_d =
\beta p_k -
\frac{1-\gamma}{1-\gamma z^{-1}}
\left(\beta p+S^\top\tau+C^\top\dot q-g\right),
\qquad p=M\dot q.
\]

For the constant-inertia scalar hinge with modeled internal torque \(h(q,\dot
q)\), the implementation becomes

\[
\hat\tau_\mathrm{ext}
=\beta I\dot q_k
-\mathrm{LPF}\left(\beta I\dot q+\tau_a-h\right).
\]

It therefore does not require measured acceleration. The experiment also
reports a forward-Euler implementation of the classical continuous momentum
observer and a finite-difference inverse-dynamics residual as comparison
baselines.

The MIT paper used a 15 Hz cutoff at 1 kHz in its force-estimation comparison.
It reported 8.7 N swing RMS error for the continuous-time derivation implemented
discretely versus 4.1 N for the fully discrete observer in that simulation.
Its later 99.3% / 4--5 ms contact-detection result belongs to the complete
Cheetah 3 fusion of force, foot-height and gait-phase priors; those numbers are
not acceptance thresholds for this one-link experiment.

Reference:
- G. Bledt, P. M. Wensing, S. Ingersoll, S. Kim,
  *Contact Model Fusion for Event-Based Locomotion in Unstructured Terrains*,
  ICRA 2018, DOI 10.1109/ICRA.2018.8460904.
- MIT author manuscript: http://hdl.handle.net/1721.1/120350

## Open-source comparators

The initial implementation stays dependency-light, but these are useful
whole-body comparators rather than code to copy blindly:

- \`mlisi1/haptiquad\`: Pinocchio/Eigen momentum residual and external-force
  estimation for floating-base robots.
- \`isri-aist/mc_external_forces_observer\`: \`mc_rtc\` observer with
  force-sensor and generalized-momentum modes, and explicit torque-source
  choices.
- \`ShahramKhorshidi/system_identification\`: physically consistent inertial
  identification with contact-nullspace projection, LMI/SDP and NLS variants.

A newer algorithmic comparator is Zhou et al.,
*Simultaneous Collision Detection and Force Estimation for Dynamic Quadrupedal
Locomotion*, ICRA 2025, which uses an interacting multiple-model Kalman filter
to estimate both contact modes and external force from encoders/dynamics. It is
a research comparison point, not a reason to add another estimator stack before
the residual baseline is characterized.

## Reproduce

Build the existing native co-simulation library and generated MuJoCo model:

\`\`\`bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python -m pip install -r examples/mujoco/requirements.txt
\`\`\`

Run only this research lane:

\`\`\`bash
python examples/mujoco/contact_estimation.py \\
  --output results/contact_estimation
\`\`\`

The output includes:

- \`summary.json\`: identified parameters, independent inertia truth, free-space
  residual RMS, observer/contact torque RMSE and correlation, contact
  precision/recall/delay, and the force-observability boundary.
- \`contact_trace.csv\`: actual-engine contact truth alongside the discrete
  momentum observer, classical observer and finite-difference residual.

The full MuJoCo verification tool also gates this lane:

\`\`\`bash
python tools/verify_mujoco.py --backend osmesa \\
  --output results/mujoco_acceptance
\`\`\`

## Mapping to \`tinmanlab/figure\`

The current \`figure\` headless integration exposes \`JointTorqueModel\` at the
plant boundary:

- \`applied(dof, commanded, qd)\` changes the torque actually applied.
- \`measured(dof, applied, qd)\` changes the torque reported to the estimator.

The actuator research lane should be used to decide what that hook represents
before adding a contact detector. In particular, commanded torque, motor-current
torque and transmitted joint torque are not interchangeable when rotor inertia,
gear compliance/backlash, friction and sensor delay are present.

A defensible transfer sequence is:

1. validate the scalar residual against MuJoCo generalized contact-torque truth;
2. expose/characterize the actuator-side torque measurement that best matches
   the intended hardware observable;
3. port the discrete momentum residual to a multi-DoF rigid-body model;
4. map residual to foot wrench using the foot Jacobian under explicit contact
   assumptions;
5. compare against \`figure\`'s MuJoCo foot-wrench truth;
6. only then add contact probability/fusion if the force channel alone is
   insufficient.

This lane is simulation research evidence. It does not establish calibrated
hardware torque sensing, real-foot force accuracy, whole-body observability, or
the complete MIT contact-fusion performance.
