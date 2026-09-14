# Quickstart

## Nothing to install

Open **https://tinmanlab.github.io/actuator/** in a recent desktop browser. Once **Start simulation** is enabled, click it. Drag the model to orbit, scroll to zoom, and use **Motor**, **Board**, or **Open motor** for inspection.

Try **Track a target**, **Reject a push**, **Meet a stop**, and **Trip the driver** in that order. Presets start a new experiment and clear speed/torque/current feed-forward commands. The fault preset injects its fault at 0.7 simulated seconds. The push preset injects +4 N·m for 120 ms at the same time. Push can also be repeated manually.

## Native build

Requirements: CMake 3.20+, a C++17 compiler; Python for asset generation and analysis tests.

```bash
git clone https://github.com/tinmanlab/actuator.git
cd actuator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/qdd_sim --case contact --duration 1.6 --profile profiles/visual.ini --output results/contact
```

Linux is the directly exercised command path. WSL2 can use the Linux workflow, but is not independently runtime-verified here. Windows multi-configuration CMake generators put binaries in `build/Release/`; use `cmake --build build --config Release` and `ctest --test-dir build -C Release`. This is not a native-Windows runtime verification claim.

## Real MuJoCo viewer and recordings

```bash
python -m pip install -r examples/mujoco/requirements.txt
python examples/mujoco/run.py --case disturbance --live --output results/interactive
```

Cases: `tracking`, `disturbance`, `contact`, `impact`, `fault`. Use a new output directory for every run to preserve evidence. Engine version is pinned in the requirements file.

For reproducible Linux software rendering:

```bash
sudo apt-get install libosmesa6 ffmpeg
python tools/verify_mujoco.py --backend osmesa --record --output results/acceptance
```

macOS passive viewing can require `mjpython examples/mujoco/run.py ... --live`. EGL is an alternative on a supported GPU; OSMesa is the CI reference. Neither browser tests nor offscreen CI evidence certifies every desktop GUI interaction.

## Troubleshooting

| Symptom | Action |
|---|---|
| Downloaded HTML does not load | Use Pages or serve the **complete built `_site` directory** over HTTP. `file://` cannot load this worker-based app. |
| Engine stays on Loading | Reload, enable JavaScript/WebAssembly, and inspect the visible error and console. Never replace failed physics with an animation. |
| 3D is blank | Enable WebGL/hardware acceleration or try another recent browser. |
| Fault does not clear | Expected: it latches. Reset creates a new virtual experiment, not a real drive recovery procedure. |
| Link stops short | Check load, angular stop, mode, limits and saturation. Impedance has static error under load. |
| Surprising steady-state offset | Advanced speed and torque commands also act as feed-forward terms in position/impedance modes. Presets clear them. |
| Running is slow | Speed is a target. Fixed integration steps are preserved instead of weakening the physics to keep up. |
| MuJoCo cannot find the library | Build the C++ project first. `examples/mujoco/bridge.py` loads the C ABI library. |
| Existing results are rejected | Choose a new output folder. Evidence is not overwritten. |
| Pages is stale | Reload without cache and compare the footer source commit with the latest Pages deployment. |

## Data and interpretation

CSV retains up to eight simulated seconds at 1 kHz. Positive load resists positive output rotation. Output torque is gearbox torque; q-axis current is not an output torque sensor. Temperatures and phase duties are simulated values, not board measurements. Use native CLI traces for switching behavior and parameter studies.
