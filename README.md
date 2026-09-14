<div align="center">

# Actuator Lab
### Motor control you can see, change, and test.

**A C++ / STM32-oriented QDD motor-control bench, an interactive browser lab, and real MuJoCo experiments.**

[**▶ Launch the interactive lab**](https://tinmanlab.github.io/actuator/) · [**Watch MuJoCo experiments**](https://tinmanlab.github.io/actuator/#videos) · [**First experiment**](#your-first-experiment--30-seconds)

[![Browser lab and Pages](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml)
[![Native and MuJoCo verification](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml)
![C++17](https://img.shields.io/badge/core-C%2B%2B17-225f50)
![WebAssembly](https://img.shields.io/badge/browser-WebAssembly-526899)
[![MIT](https://img.shields.io/badge/license-MIT-64745d)](LICENSE)

[![Open the live C++ motor-control lab: 3D bench, controls, current and torque telemetry](https://tinmanlab.github.io/actuator/media/browser-lab.png)](https://tinmanlab.github.io/actuator/)

**No installation. No account. No API key. The browser computes the simulation on your machine.**

</div>

## What is this place?

Change a target or apply a load and follow the whole chain: **controller → phase current → motor torque → gearbox → moving link**. This project makes low-level motor control observable, and provides a shared C++ core for simulation and future embedded integration.

It is useful for learning FOC, comparing controllers, testing disturbance rejection, understanding drive faults, and developing a QDD servo. **It is not production-ready STM32 firmware or a hardware-validated actuator.** All included motor parameters and board geometry are illustrative.

| I want to… | Go here | What runs |
|---|---|---|
| Try a controller immediately | [**Interactive browser lab**](https://tinmanlab.github.io/actuator/#simulator) | Real C++ FOC/inverter/PMSM code compiled to WebAssembly; native 1-axis mechanics |
| Inspect the motor and board | [**3D viewer**](https://tinmanlab.github.io/actuator/#simulator) → **Motor**, **Board**, **Open motor** | The same procedural model recipe used by the MuJoCo examples |
| See full 3D contacts and falling-object impacts | [**MuJoCo film gallery**](https://tinmanlab.github.io/actuator/#videos) | Recordings of actual C++ ↔ MuJoCo closed-loop runs |
| Run and modify the source | [**Local quickstart**](docs/QUICKSTART.md) | Native C++ CLI, tests, or the MuJoCo viewer |
| Add a controller | [`CurrentAlgorithm`](include/qdd/control.hpp) · [architecture](docs/ARCHITECTURE.md) | Shared current-control interface; not a separate JavaScript controller |

## Your first experiment — 30 seconds

1. **[Open the lab](https://tinmanlab.github.io/actuator/)** and press **Start simulation**. Move the **Target angle** slider. Watch the measured angle approach the reference.
2. Press **Push +4 N·m / 120 ms**. The current and output torque react to the disturbance. Try a constant **2 N·m external load**: an impedance controller yields by approximately `load / Kp`.
3. Choose **Meet a stop**. The 0.85 rad target is beyond the 0.60 rad stop. The angle stops, but reaction torque and current remain. Then **Trip driver** to see why gate-off does not instantly remove stored energy.

Use **Pause**, **+1 ms**, and **Export CSV** to inspect a result. **Reset experiment** starts a new virtual experiment at zero state. Changing the current algorithm also resets and pauses. No controls reach physical hardware.

### Controls at a glance

| Control | What it changes |
|---|---|
| Position / impedance / velocity / torque / current mode | The outer-loop command sent to the same low-level drive |
| Target, stiffness, damping, external load | Live physical/control inputs, not animation settings |
| PI FOC / relaxed predictive current | Current-control algorithm; switching starts a fresh experiment |
| Push, angular stop, driver trip | Disturbance, unilateral angular contact, or latched virtual gate fault |
| Bench / Motor / Board / Open motor | Camera and visual inspection only — never physical parameters |
| Export CSV | Most recent 8 simulated seconds at 1 kHz with explicit units |

## Watch the actual MuJoCo experiments

**These films are recorded, not interactive.** They use full MuJoCo mechanical/contact dynamics coupled to the C++ electrical model. The live browser tab uses the native 1-axis plant instead; it does not claim to run MuJoCo in the browser.

<table>
<tr>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-tracking"><img src="https://tinmanlab.github.io/actuator/media/tracking.jpg" alt="Actual MuJoCo position tracking"/><br/><b>▶ Position tracking</b></a><br/>See current become motion.</td>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-disturbance"><img src="https://tinmanlab.github.io/actuator/media/disturbance.jpg" alt="Actual MuJoCo external load response"/><br/><b>▶ Disturbance rejection</b></a><br/>Push the driven link.</td>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-contact"><img src="https://tinmanlab.github.io/actuator/media/contact.jpg" alt="Actual MuJoCo stopper contact"/><br/><b>▶ Stopper contact</b></a><br/>Inspect reaction and compliance.</td>
</tr>
<tr>
<td><a href="https://tinmanlab.github.io/actuator/#film-impact"><img src="https://tinmanlab.github.io/actuator/media/impact.jpg" alt="Actual MuJoCo falling-object impact"/><br/><b>▶ Falling-object impact</b></a><br/>A free body hits the link.</td>
<td><a href="https://tinmanlab.github.io/actuator/#film-fault"><img src="https://tinmanlab.github.io/actuator/media/fault.jpg" alt="Actual MuJoCo driver fault"/><br/><b>▶ Driver fault</b></a><br/>Gates off; mechanics continues.</td>
<td><b>Evidence, not a pose animation.</b><br/><br/>Videos are generated from actual engine states. Each build publishes <a href="https://tinmanlab.github.io/actuator/media/evidence.json">acceptance results and video hashes</a>.<br/><br/><a href="https://tinmanlab.github.io/actuator/#learn">Open the four guided labs →</a></td>
</tr>
</table>

[![Actual C++ and MuJoCo contact recording; click to play the full film](https://tinmanlab.github.io/actuator/media/contact.gif)](https://tinmanlab.github.io/actuator/#film-contact)

GitHub README sanitization prevents a live iframe or a dependable HTML video player here. **Click any preview** for the embedded player on Pages. Those public videos do not require downloading a GitHub Actions artifact or signing in.

## Run locally

### Native C++ — no graphics dependency

```bash
git clone https://github.com/tinmanlab/actuator.git
cd actuator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/qdd_sim --case contact --duration 1.6 \
  --profile profiles/visual.ini --output results/contact
```

### Full MuJoCoCo viewer

```bash
python -m pip install -r examples/mujoco/requirements.txt
python examples/mujoco/run.py --case disturbance --live \
  --output results/my-first-mujoco-run
```

Use a new result directory for each run. On macOS, the passive viewer may require `mjpython` instead of `python`. See [platform notes, recording, and troubleshooting](docs/QUICKSTART.md).

### Build the browser lab

```bash
# Install Emscripten, Node.js, and Python first.
npm install --prefix web
python tools/build_site.py
bash tools/build_wasm.sh
python -m http.server 8765 --directory _site
# Open http://localhost:8765 — not file://
```

The live simulator works before films are generated. To generate the complete gallery, run the MuJoCo acceptance suite with `--record`, then `tools/site_media.py`; see [the full web build](docs/WEB_LAB.md). Graphics and WASM dependencies are served from the same site, with no runtime CDN dependency.

## Under the hood

```text
Command → servo loop → PI FOC / predictive current → SVPWM
       → inverter → PMSM → compliant QDD gearbox → load
       ← sampled current + encoder feedback ←──────────┘
                         independent comparator/BREAK → gate-off
```

The C++ model includes electrical d/q dynamics, back-EMF, nonideal inverter behavior, sensing/quantization/delay, DC-link regeneration, thermal state, a two-inertia drivetrain, and fault latching. The command/measurement abstraction separates controller logic from STM32 board-specific I/O.

| Path | Timing / ownership | Claim boundary |
|---|---|---|
| Browser live lab | 20 kHz FOC, ≤5 μs electrical integration, 1 kHz telemetry, Web Worker | Averaged inverter, native 1-axis mechanics and angular stop; no 3D impact engine |
| Native CLI | Averaged or switched inverter; offline diagnostics and sweeps | Synthetic SIL, not dyno correlation |
| MuJoCo examples | 50 μs electrical/mechanical exchange; MuJoCo owns mechanical DOFs and contacts | Tested example scenarios, not universal co-simulation convergence |
| STM32 port contract | Shared control core + board I/O boundary | Not a flashable BSP, verified timing result, or safety-certified power stage |

**Fixed-step simulation time is not a hard real-time guarantee.** A slower computer runs more slowly; the simulation does not enlarge its numerical timestep. Plots are not switching-ripple oscilloscopes.

## Find your way around

- [Quickstart and troubleshooting](docs/QUICKSTART.md) — first run, local viewer, recording, OS notes.
- [Browser lab contract](docs/WEB_LAB.md) — controls, data, build, deployment, limitations.
- [Architecture and extension points](docs/ARCHITECTURE.md) — what owns physics, mechanics, and hardware I/O.
- [Electrical–mechanical coupling contract](docs/CO_SIMULATION_CONTRACT.md) — avoid double-counting inertias.
- [Current verification](https://github.com/tinmanlab/actuator/actions) — exact-commit native, MuJoCo, and browser checks.

`src/` · C++ physics and control | `include/qdd/` · public interfaces | `stm32/` · port boundary | `profiles/` · synthetic configurations | `examples/mujoco/` · full mechanics | `web/` · browser interface | `tests/` · regression tests

## Safety & license

**Do not flash this host simulator onto a power stage.** Hardware overcurrent paths, gate timing, sensor polarity, watchdogs, thermal limits, and real motor parameters need separate validation. The depicted board is not an electrical schematic or fabrication-ready PCB. The 6:1 transmission is a compliant lumped model, not meshed gear-tooth contact.

Project code is [MIT licensed](LICENSE). Three.js is bundled with its MIT license. MuJoCo and other dependencies retain their own licenses. Model geometry is procedurally generated in this repository.
