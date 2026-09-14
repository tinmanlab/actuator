# Bidirectional contact and implementation guides

The browser bench is a one-axis reduction, not a Three.js collision engine. Its
finite stopper must block both directions and every revolution. The previous
`q > 0.60` test only covered positive unwrapped motion and admitted reverse
passage through the rendered box.

`tools/stop_geometry.py` reduces the existing MJCF tip sphere and link capsule
against the box to a blocked angular interval. The generated header is build
output, not a second editable geometry owner. Unsupported fixtures fail the
build rather than silently using unrelated constants. `web/stop.hpp` anchors
the connected free arc at enable time, applies signed spring/damper load torque,
and retains that arc during contact. The default free arc is approximately
[-5.181270, 0.600000] rad. Obstacle insertion into an occupied solid interval is
rejected without teleportation or a fatal engine error. Disable the stop to
inspect unconstrained rotations; re-enable only after moving clear.

The reaction is evaluated in the existing electrical/mechanical substep loop.
It is subtracted as load, not imposed as a pose clamp. Stiffness is 6000 N m/rad
and damping 25 N m s/rad; this changes the reduced contact response, not the
motor or FOC gains. Small compliant deflection remains intentional. It does
not establish zero penetration, general 3D contacts, contact-rate convergence,
mechanical certification, or hardware safety.

## Regression evidence to reproduce

- `browser_reverse_contact_roundtrip`: opposite-face reaction, release and
  return, periodicity across 41 turns, occupied-enable rejection, and actual
  facade trials at +/-1, +/-3 and +/-6 rad/s for both current algorithms.
- `stop_geometry_and_guide_contracts`: sphere/box tangency, segment geometry,
  complete command and telemetry tables, internal links and exact example text.
- `documented_live_api_example`: build and run the same C++ example shown online.
- `web/smoke.cjs`: real WASM reverse/return, nonfatal enable rejection, optional
  collision envelopes, guide navigation, keyboard pipeline controls, coordinate
  calculation, responsive tables, and the existing video/control acceptance.

The regression bound is observed angular deflection below 0.03 rad in the
specified velocity trials. It is not a universal bound under arbitrary energy
or injected loads. Native and WASM executions must both pass before deployment.

## Reading surfaces

- `web/control.html`: control engineering, pipeline blocks, units, equations,
  timing, limiting, protection, and contact scope.
- `web/api.html`: actual ownership, compiled example, all commands/telemetry,
  Worker protocol, C11 controller ABI, and external MuJoCo boundary.
- `web/guide.js`: reading interactions only; its rotating-frame calculation is
  clearly separated from live telemetry. Source links bind to `build.json`.

The live chart uses plant-state currents and angles, not raw ADC samples. A
control diagram must retain this distinction and must not imply that geometry
is validated CAD, that browser mechanics equals MuJoCo, or that a simulation
period is a measured STM32 ISR execution time.
