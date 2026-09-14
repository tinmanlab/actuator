# Quickstart

## Nothing to install

Open **https://tinmanlab.github.io/actuator/** in a recent desktop browser. Wait for **Ready · C++ WebAssembly**, then click **Start simulation**. Drag the model to orbit, scroll to zoom, and use **Motor**, **Board**, or **Open motor** for inspection.

Try **Track a target**, **Reject a push**, **Meet a stop**, and **Trip the driver** in that order. Presets start a new experiment. The fault preset injects its fault at 0.7 simulated seconds. The push preset injects +4 N·m for 120 ms at the same time. Push can also be repeated manually.

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

Linux and WSL2 are the directly exercised command paths. Windows multi-configuration CMake generators put binaries in `build/Release/`; use `cmake --build build --config Release` and `ctest --test-dir build -C Release`. This note is not a native-Windows runtime verification claim.

## Real MuJoCo viewer and recordings

```bash
python -m pip install -r examples/mujoco/requirements.txt
python examples/mujoco/run.py --case disturbance --live --output results/interactive
```

The engine version is pinned in the requirements file. Cases: `tracking`, `disturbance`, `contact`, `impact`, `fault`. Output directories must be new to protect evidence.

For offscreen Linux recording, install ffmpeg and a suitable OpenGL backend. A reproducible software-rendering path is:

```bash
sudo apt-get install libosmesa6 ffmpeg
python tools/verify_mujoco.py --backend osmesa --record --output results/acceptance
```

macOS passive viewing can require `mjpython examples/mujoco/run.py ... --live`. EGL is an alternative backend on a supported GPU; OSMesa is the CI reference. Neither the browser nor offscreen CI evidence certifies all desktop GUI interactions.

## Troubleshooting

| Symptom | Action |
|---|---|
| Downloaded HTML does not load the engine | Use the hosted Pages URL or serve the **complete built `_site` directory** over HTTP. `file://` cannot load this worker-based application. |
| Engine stays on Loading | Reload, check that JavaScript/WebAssembly are enabled, and inspect the visible error banner and browser console. Do not replace failed physics with an animation. |
| 3D is blank | Enable WebGL/hardware acceleration or try another recent browser. The text/CSV and MuJoCo films have separate purposes. |
| The fault does not clear | Expected: it latches. Reset starts a new virtual experiment. This is not a real drive recovery procedure. |
| The link stops short of the target | Check external load, angular stop, mode, torque/current limits, and saturation. In impedance mode a static error under load is expected. |
| Running is slower than the speed setting | The speed is a target. The fixed physical timestep is preserved instead of weakening the integrator to keep up. |
| MuJoCo says a library is missing | Build the native project first. The generated library is loaded through the C ABI in `examples/mujoco/bridge.py`. |
| Existing results are rejected | Choose a new output folder. Do not overwrite a previous experiment's evidence. |
| Pages has an old view | Reload without cache. Check the source commit in the footer against the Pages workflow deployment. |

## Data and interpretation

Browser CSVs retain up to eight seconds of 1 kHz telemetry. Positive load resists positive output rotation. Output torque is gearbox torque; q-axis current is not a measured output torque sensor. Simulated temperatures and phase duties are functional values, not board measurements. For full switching traces and parameter studies, use the native CLI.
