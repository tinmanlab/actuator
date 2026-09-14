<div align="center">

# Actuator Lab
### Motor control you can see, change, and test.

**A C++ / STM32-oriented QDD motor-control bench, an interactive browser lab, and actual MuJoCo experiments.**

[**▶ Launch the interactive lab**](https://tinmanlab.github.io/actuator/) · [**Watch MuJoCo experiments**](https://tinmanlab.github.io/actuator/#videos) · [**First experiment**](#your-first-experiment--30-seconds)

[![Browser lab and Pages](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml)
[![Native and MuJoCo verification](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml)
![C++17](https://img.shields.io/badge/core-C%2B%2B17-225f50)
![WebAssembly](https://img.shields.io/badge/browser-WebAssembly-526899)
[![MIT](https://img.shields.io/badge/license-MIT-64745d)](LICENSE)

[![Open the live C++ motor-control lab: 3D bench, controls, current and torque telemetry](https://tinmanlab.github.io/actuator/media/browser-lab.png)](https://tinmanlab.github.io/actuator/)

**No installation. No account. No API key. Your browser computes the simulation locally.**

</div>

## What is this place?

Change a target or apply a load and follow the whole chain: **controller → phase current → motor torque → gearbox → moving link**. This project makes low-level motor control observable and provides a shared C++ core for simulation and future embedded integration.

Use it to learn FOC, compare controllers, test disturbance rejection, understand drive faults, and develop a QDD servo. **It is not production-ready STM32 firmware or a hardware-validated actuator.** Motor parameters and board geometry are illustrative.

| I want to… | Go here | What runs |
|---|---|---|
| Try a controller immediately | [**Interactive lab**](https://tinmanlab.github.io/actuator/#simulator) | Existing C++ control and plant compiled to WebAssembly; native 1-axis mechanics |
| Inspect the motor and board | [**3D viewer**](https://tinmanlab.github.io/actuator/#simulator) → Motor / Board / Open motor | The same procedural model recipe as the MuJoCo examples |
| See full 3D contacts and impacts | [**MuJoCo films**](https://tinmanlab.github.io/actuator/#videos) | Actual C++ ↔ MuJoCo recordings |
| Run and modify source | [**Quickstart**](docs/QUICKSTART.md) | Native C++ CLI, tests, and local MuJoCo viewer |
| Add a controller | [`CurrentAlgorithm`](include/qdd/control.hpp) · [architecture](docs/ARCHITECTURE.md) | Shared current-control interface, not a separate JavaScript implementation |

## Understand the controller and its code

[**Control engineering & interactive pipeline**](https://tinmanlab.github.io/actuator/control.html) · [**Code & API reference**](https://tinmanlab.github.io/actuator/api.html)

Follow a command through outer loops, current references, Clarke/Park transforms, PI/predictive FOC, SVPWM, the inverter, motor and sensor feedback. The guide includes signal units, sampling timelines, saturation, safety boundaries, and an interactive rotating-frame example. The API guide maps the actual C++ sources, all browser command/telemetry fields, Worker messages, C11 controller ABI, MuJoCo coupling, and a C++ example built by CTest.

[**Try reverse contact**](https://tinmanlab.github.io/actuator/?experiment=reverse): the finite stop now reacts at both faces on every revolution. Turn on **Collision envelopes** to inspect the reduced collision geometry. Small spring deflection remains intentional; this is not a general browser 3D collision solver. Enabling a stop inside its occupied volume is rejected rather than teleporting the link.

## Your first experiment — 30 seconds

1. **[Open the lab](https://tinmanlab.github.io/actuator/)** and press **Start simulation**. Move **Target angle**. Watch output angle follow the reference.
2. Press **Push +4 N·m / 120 ms**. Current and torque react. Then set a constant **2 N·m load**: an impedance controller yields by approximately `load / Kp`.
3. Choose **Meet a stop**. The 0.85 rad target is beyond the 0.60 rad stop. The angle stops while reaction torque and current remain. **Trip driver** to see why gate-off does not instantly remove stored energy.

Use **Pause**, **+1 ms**, and **Export CSV** to inspect a result. **Reset experiment** creates a fresh virtual instance. Algorithm changes reset and pause. No controls reach physical hardware.

### Controls at a glance

| Control | What it changes |
|---|---|
| Position / impedance / velocity / torque / current mode | Outer-loop command sent to the same low-level drive |
| Target, stiffness, damping, external load | Actual control/plant inputs, not animation settings |
| PI FOC / relaxed predictive current | Current algorithm; changing it starts a new experiment |
| Push / angular stop / driver trip | Disturbance, two-sided periodic stop, or latched gate fault |
| Bench / Motor / Board / Open motor | Visual inspection only, never physical parameters |
| Export CSV | Latest 8 simulated seconds at 1 kHz, with explicit units |

Advanced speed and torque commands also act as feed-forward terms in position/impedance modes. They start at zero; presets clear them so a previous experiment cannot silently bias the next one.

## Watch actual MuJoCo experiments

**These films are recorded, not interactive.** MuJoCo computes the mechanical/contact dynamics coupled to the C++ electrical model. The live browser lab uses the native 1-axis plant, not browser MuJoCo.

<table>
<tr>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-tracking"><img src="https://tinmanlab.github.io/actuator/media/tracking.jpg" alt="Actual MuJoCo position tracking"/><br/><b>▶ Position tracking</b></a><br/>See current become motion.</td>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-disturbance"><img src="https://tinmanlab.github.io/actuator/media/disturbance.jpg" alt="Actual MuJoCo external load response"/><br/><b>▶ Disturbance rejection</b></a><br/>Push the driven link.</td>
<td width="33%"><a href="https://tinmanlab.github.io/actuator/#film-contact"><img src="https://tinmanlab.github.io/actuator/media/contact.jpg" alt="Actual MuJoCo stopper contact"/><br/><b>▶ Stopper contact</b></a><br/>Inspect reaction and compliance.</td>
</tr>
<tr>
<td><a href="https://tinmanlab.github.io/actuator/#film-impact"><img src="https://tinmanlab.github.io/actuator/media/impact.jpg" alt="Actual MuJoCo falling-object impact"/><br/><b>▶ Falling-object impact</b></a><br/>A free body hits the link.</td>
<td><a href="https://tinmanlab.github.io/actuator/#film-fault"><img src="https://tinmanlab.github.io/actuator/media/fault.jpg" alt="Actual MuJoCo driver fault"/><br/><b>▶ Driver fault</b></a><br/>Gates off; mechanics continues.</td>
<td><b>Evidence, not a pose animation.</b><br/><br/>Each build publishes <a href="https://tinmanlab.github.io/actuator/media/evidence.json">acceptance results and video hashes</a>.<br/><br/><a href="https://tinmanlab.github.io/actuator/#learn">Four guided labs →</a></td>
</tr>
</table>

[![Actual C++ and MuJoCo contact recording; click for the full film](https://tinmanlab.github.io/actuator/media/contact.gif)](https://tinmanlab.github.io/actuator/#film-contact)

GitHub README does not execute a live iframe or this application's scripts. **Click any preview** to open embedded players on Pages. The public videos need neither an Actions-artifact download nor a sign-in.

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

### Full MuJoCo viewer

```bash
python -m pip install -r examples/mujoco/requirements.txt
python examples/mujoco/run.py --case disturbance --live \
  --output results/my-first-mujoco-run
```

Use a new output directory for every run. macOS passive viewing may require `mjpython` instead of `python`. See [platform notes, recording and troubleshooting](docs/QUICKSTART.md).

### Browser development

```bash
# Install Emscripten, Node.js and Python first.
npm install --prefix web
python tools/build_site.py
bash tools/build_wasm.sh
python -m http.server 8765 --directory _site
# Open http://localhost:8765, not file://
```

For films, run the MuJoCo acceptance suite with `--record`, then `tools/site_media.py`. See [the full site build](docs/WEB_LAB.md). Rendering/WASM assets are hosted together, with no runtime CDN dependency.

## Under the hood

```text
Command → servo loop → PI FOC / predictive current → SVPWM
       → inverter → PMSM → compliant QDD gearbox → load
       ← sampled current + encoder feedback ←──────────┘
                         comparator / BREAK → gate-off
```

The C++ model includes d/q electrical dynamics, back-EMF, inverter nonidealities, sensing/quantization/delay, DC-link regeneration, thermal state, a two-inertia drivetrain and fault latching. The command/measurement boundary separates control logic from board-specific I/O.

| Path | Timing / ownership | Boundary |
|---|---|---|
| Browser live | 20 kHz FOC, ≤5 μs electrical substeps, 1 kHz telemetry in a Worker | Averaged inverter, native 1-axis mechanics and angular stop |
| Native CLI | Average or switched inverter; diagnostics and sweeps | Synthetic SIL, not dyno correlation |
| MuJoCo | 50 μs electrical/mechanical exchange; engine owns both mechanical DOFs and contacts | Tested examples, not universal coupling convergence |
| STM32 port | Shared core and board I/O contract | Not a flashable BSP, verified MCU timing, or certified power stage |

Fixed-step simulation time is **not a hard real-time guarantee**. A slower computer runs more slowly instead of enlarging the numerical timestep. Telemetry plots are not switching-ripple oscilloscopes.

## Find your way around

[Quickstart](docs/QUICKSTART.md) · [Browser lab](docs/WEB_LAB.md) · [Architecture](docs/ARCHITECTURE.md) · [Coupling contract](docs/CO_SIMULATION_CONTRACT.md) · [Exact-commit verification](https://github.com/tinmanlab/actuator/actions)

`src/` physics/control · `include/qdd/` interfaces · `stm32/` port boundary · `profiles/` configurations · `examples/mujoco/` mechanics · `web/` browser · `tests/` regression tests

## Safety and license

**Do not flash this host simulator onto a power stage.** Hardware overcurrent paths, gate timing, sensor polarity, watchdogs, thermal limits and real motor parameters need separate validation. The illustrated board is not a schematic or fabrication-ready PCB. The 6:1 transmission is a compliant lumped model, not gear-tooth contact.

Project code is [MIT licensed](LICENSE). Three.js includes its MIT license; MuJoCo and other dependencies retain their licenses. Geometry is procedurally generated in this repository.
