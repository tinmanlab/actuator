# Interactive powertrain contract

The **Powertrain** page recalculates three isolated experiments using `web/experiments.cpp` and the existing `Drive`, `Plant`, `Inverter`, `DcLink` and `Sensors` classes. The JavaScript transports results and draws graphs; it is not a second motor model. Each experiment is independent of the live joint. Changing inputs marks results stale until recalculation. Playing a completed switching trace is replay, not further integration.

## Reproduce and modify

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/qdd_electronics_example > motor-map.csv
```

Build the web site using `docs/WEB_LAB.md`; Emscripten exports the same C API in `web/experiments.h`. `experiment-worker.js` invokes `exp_run(kind, iq, rpm, noise, bandwidth, deadtime_ns, ambient, rotor_viscous)`, then copies `exp_rows(table)` rows through `exp_get(table,row,column)`. Success is 1; invalid/failed requests return 0 and preserve the last complete result. Out-of-range reads return NaN. This host-side API allocates result buffers; it is **not an ISR interface**. The existing 11-command / 31-field live-joint API is unchanged.

Bounds: |Iq| ≤15 A, |motor RPM| ≤4000, capture-noise standard deviation 0–0.5 A, analog bandwidth 1–40 kHz, dead time 0–450 ns (the existing inverter requires less than 1% of the 50 μs period), ambient 0–60 °C, rotor viscous drag 0–0.003 N·m·s. Iq uses amplitude-invariant dq scaling, not phase RMS. All inputs must be finite. Thermal UI additionally requires nonnegative current; heating depends on its square.

## Three different experiments

**Kind 0, switched dynamometer.** Fresh 48 V motor model and controller; 120 ms total, 20 kHz FOC/PWM, 30 ms imposed-speed ramp from 4 ms, current command at 6 ms. The speed source is an ideal dynamometer, not free acceleration. Mechanical DOFs are locked inside `Plant` and their angle/speed are imposed analytically once, avoiding double integration. Rotor angle is set at electrical substep midpoints; the electrical integration is ≤1 μs and splits at PWM/dead-time edges and ADC acquisition. This fixed-resolution split is not claimed to be universally converged.

The ADC is captured halfway through each 50 μs period, delivered through the existing sensing delay, then angle-aligned in `Drive`. The analog filter follows the true phase current; this model adds noise/offset at ADC capture **after** that filter, then quantizes. Thus reducing analog bandwidth does not directly remove the additive capture noise. Amplifier gain is abstracted in ampere-calibrated units. No low-side sampling-window reconstruction, Vgs waveform, EMI, ringing, gate charge or reverse recovery. The dyno uses `Drive` software protections; the live joint's separate `FastTrip` bench is not silently claimed here.

**Kind 1, prescribed-current thermal diagnostic.** Reuses copper heating, temperature-dependent R and winding/case heat equations for 600 physical seconds with 10 ms integration. Current is prescribed, not produced by a closed-loop inverter. FET temperature is a conduction-loss proxy. Current limiting is intentionally bypassed; use the separately labeled derating experiment for protection. No iron/switching heat or winding hot spots.

**Kind 2, motor map.** 28 fresh averaged-inverter current-loop runs: Iq commands 2/5/10/15 A, imposed motor shaft speeds 0/300/600/1200/1800/2400/3600 RPM. The last 20 ms of each 120 ms experiment supply time-weighted power means. Settings for noise, bandwidth, dead-time approximation, ambient and rotor friction are retained; the single-point RPM/Iq inputs do not set this fixed grid. Shaft torque is electromagnetic torque minus smooth Coulomb and viscous drag. No 6:1 gearbox is included.

Efficiency boundary: **inverter DC terminals → motor shaft**. η=Pshaft/PDC is shown only for positive input/output power, no fault, mean-current error <max(0.15 A,3% of command), stored-energy rate <2% of max(1 W,|PDC|), and η≤1. Unqualified points export `eta=-1`, render as gaps, and retain saturation, current error and fault fields. Saturated points generally fail tracking; saturation fraction is reported independently. This is a synthetic cold-start map, not continuous torque or a commercial efficiency rating. Iron, switching-energy and auxiliary losses remain omitted. Numerical residual is displayed, not relabeled as heat.

## Result tables and timing

Every internal row has capacity 48; unused slots are zero and are **not measured values**. CSV exports use only the applicable columns described below.

Tables **0** (control, every 50 μs) and **1** (last 2 ms of switching) share indexes:

| Index | Meaning / units |
|---|---|
| 0–2 | sample time s, applied Iq reference A, predicted actuation electrical angle rad |
| 3–9 | true Id/Iq A, true Ia A, analog filtered Ia A, raw held ADC Ia A, corrected/aligned observed Id/Iq A |
| 10–13 | limited Vd/Vq V, inverse-Park Vα/Vβ V |
| 14–16 | table 0: commanded average phase-neutral voltage; table 1: bridge phase-neutral voltage V |
| 17–24 | A/B/C duty, DC voltage V, target RPM (not ramped speed), shaft torque N·m, winding/FET °C |
| 25–33 | table 1 only: AH/AL/BH/BL/CH/CL logic, DC/AC/inverter-loss W |
| 34–43 | shaft/copper/friction W, gate enable, state, fault, plant electrical angle rad, current age s at controller tick, current limit A, voltage saturation |
| 44–47 | true Ib/Ic A; table 1 only: carrier [0,1], integration interval s |

Control rows evaluate true/filtered channels at the control tick; ADC and PI values come from their stated earlier sample/hold and control instant. Microstep rows evaluate the bridge at the midpoint; floating-diode steps use the existing implicit endpoint current. Do not infer nanosecond continuous-time current samples from event timestamps. Gate bits are logical commands, not Vgs or a declaration that an OFF transistor's diode carries no current.

**Table 2:** time s; winding/case/FET °C; phase resistance Ω; copper W.

**Table 3:** RPM; Iq command A; mean motor shaft torque N·m; DC W; shaft W; η or -1; qualified flag; mean Iq A; current error A; saturation fraction; fault; copper/inverter/friction W; stored-energy rate W; numerical residual W; final winding °C.

## Media provenance and auto-start

`site_media.py` selects events from an accepted MuJoCo trace, verifies its hash, and uses the renderer's explicit playback-speed metadata to trim video. Tracking selects first motion, disturbance selects the load pulse, contact/impact select intended-contact force, and fault selects the fault channel. Full source MP4s remain available. Loops replay a bounded event and may jump at restart; frames are never reversed or represented as perpetual physical motion. The README tour concatenates all five excerpts sequentially.

The joint starts after loading when the page is visible. Pause, reset and algorithm-change semantics remain explicit; `?paused=1` disables entry auto-start. These are simulated commands only. Replaying a movie, running WASM, and sending commands to a physical device are distinct activities.
