# Architecture

The repository separates three ownership boundaries.

**Control (`qdd_core`).** `Drive` implements calibration, state/fault handling, current limits, outer modes, and current control. `CurrentAlgorithm` is the extension point; `PiFoc` and `PredictiveCurrent` are the built-ins. Controller use in simulation does not imply board-specific timing verification.

**Electrical and mechanical plant (`qdd_host`).** `Plant`, `Inverter`, `DcLink`, `Sensors`, and `FastTrip` provide the synthetic plant and signal path. Native batch scenarios live in `src/bench.cpp`; the browser's incremental facade is in `web/live.cpp`. Both reuse the same component implementations. Average versus switched inverter fidelity must remain explicit.

**External mechanics (`qdd_external`).** `src/external.cpp` exchanges state/torques with the MuJoCo Python runner through a C ABI. MuJoCo owns both mechanical DOFs and contacts, and the C++ external mode does not integrate their inertias again. See [the coupling contract](CO_SIMULATION_CONTRACT.md).

## Rendering and educational content

`tools/build_bench_model.py` is the single procedural geometry recipe. MuJoCo compiles its MJCF and OBJ assets. The browser site exports the same recipe for Three.js through `tools/build_site.py`. Static decoration has no mechanical authority. GPIO-looking board geometry is not a schematic.

The browser's trace follows its live native-mechanical state. The video gallery is recorded from actual MuJoCo runs. Keeping these paths explicit prevents a rendered pose from being mistaken for physics verification.

## Extending a controller

Implement `CurrentAlgorithm::voltage`, `track`, and `reset`, preserving current/voltage units and conventions. `Drive::use_algorithm` only swaps while Disabled or Ready; the caller owns its lifetime. Add native analytical/regression tests before exposing it in the browser. Do not hide a stability problem by changing the model or lowering acceptance requirements.

## STM32

`stm32/port_contract.hpp` defines the board I/O boundary. It is not a CubeMX project or a validated interrupt routine. PWM/ADC synchronization, independent comparator/BREAK, sensor polarity, power-stage timing and physical bring-up remain separate hardware work.
