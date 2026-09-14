# Signal experiments: one question, one change

Read the [signal guide](https://tinmanlab.github.io/actuator/physics.html) for equations, plots and model limits. This tutorial owns only the **reproduction steps**; the [API guide](https://tinmanlab.github.io/actuator/api.html) owns the interface.

## 1. Run the unchanged baseline

Build using [Quickstart](../QUICKSTART.md), then run:

```bash
./build/qdd_signal_lab results/signals-baseline
```

The example refuses to overwrite a nonempty folder. Its code is `examples/signal_lab.cpp`; each closed-loop experiment calls existing `run_bench`, with `sensor.linear_filter=true`. It does not introduce a new controller.

| Output | Question | Interpretation |
|---|---|---|
| `current.csv` | Does the 4 A step track? | Reference/sensed dq are at `boundary_time_s`; true current at `time_s`. |
| `modulation.csv` | How does common mode change duty? | Native SVPWM function sweep, not a motor trajectory. |
| `pwm_edges.csv` | What happens during 150 ns dead time? | Gates/voltages apply to the preceding interval; currents are endpoints. Do not smooth the gate edges. |
| `gate_off.csv` | Does current vanish when gates turn off? | Fault injection at 80 ms; diode current extinction, not a hardware timing test. |
| `thermal.csv` | How do winding and case heat differently? | Prescribed 12 A, locked shafts, 600 s. No drive limiter in this diagnostic. |
| `derating.csv` | Does a hot drive limit current? | Starts at 105°C; it is not a cold-start heating experiment. |
| `velocity_power.csv`, `friction_power.csv` | How much input power does more drag cost? | Same 3 rad/s target and 1 N·m load; only output viscous drag changes. |
| `regeneration_power.csv` | Where does braking energy go? | Negative DC input means power returns to the bus; not negative efficiency. |

Each run also saves its effective configuration JSON. `experiment.json` states the thermal fixture type, power boundary and a qualified motoring window. `_power.csv` rows are **period averages**, not simultaneous raw samples: bridge DC/AC/loss use the same constitutive evaluation state; copper/friction/load use trapezoidal substep quadrature. The energy residual remains visible.

## 2. Change exactly one assumption

In `examples/signal_lab.cpp`, compare output viscous drag `0.02` with `0.06` N·m·s/rad. Rebuild and write a new folder. Compare load output, DC input and the residual over 0.8–1.0 s, not at the acceleration spike. Do not advertise this low-power synthetic ratio as a motor efficiency map.

For sensing, change `base.sensor.current_delay_cycles` and compare `boundary_iq_A` to `sensor_iq_A`. Existing failure checks may reject the run if a new condition trips; investigate the fault instead of deleting the guard. Use native high-rate traces for current-loop tuning; the live 1 kHz graph cannot resolve PWM ripple.

## 3. Package the actual data for Pages

```bash
python tools/build_signal_data.py --exe build/qdd_signal_lab --out _site/data
```

This reruns the baseline, preserves raw CSV/configurations in a ZIP and writes compact plot JSON with file, source and executable SHA-256 hashes. Power curves use explicitly labeled 1 ms block means. Thermal and gate traces are not smoothed. The Pages workflow executes this step; it never substitutes fake curves when generation fails.

## 4. Check before interpreting

```bash
ctest --test-dir build -R 'physical_input|native_signal_data' --output-on-failure
```

Tests include finite/increasing traces, phase-current/neutral identities, non-overlapping gates, same-state inverter power balance, thermal response, guarded motoring interpretation and SVPWM line-voltage preservation. These software checks do not replace dyno, sensor calibration, thermal measurements or safe power-stage commissioning.
