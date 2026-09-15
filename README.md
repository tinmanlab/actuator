<div align="center">

# Actuator Lab
### See the current. Feel the load. Explain the loss.

**A C++ motor-control workbench for learning and testing a QDD servo—before touching power hardware.**

[**Launch the live lab →**](https://tinmanlab.github.io/actuator/) · [Signals & losses](https://tinmanlab.github.io/actuator/physics.html) · [Code & API](https://tinmanlab.github.io/actuator/api.html)

[![Browser lab and Pages](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/pages.yml)
[![Native and MuJoCo verification](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml/badge.svg)](https://github.com/tinmanlab/actuator/actions/workflows/verify.yml)

[![Short sequential tour of actual MuJoCo tracking, loads, contact, impact and driver trip](https://tinmanlab.github.io/actuator/media/feature-tour.gif)](https://tinmanlab.github.io/actuator/)

**No installation, login or API key. The live joint starts automatically; pause whenever needed.**

*GIF: short event excerpts from actual C++–MuJoCo recordings. Restarting the loop replays the event.*

</div>

## Why this is useful

A moving model does not explain a motor. This lab lets you follow **command → FOC → PWM → inverter → phase current → torque → motion**, then ask where the energy went.

The first screen connects **Command → FOC → SVPWM / inverter → Output → Sensor feedback** to the same running joint. Select one stage for its live values; pause to inspect without changing the physics.

Change a target, push the output, meet a stop or trip the driver. Inspect current and torque, open the motor housing, pause, advance 1 ms and export CSV. The browser compiles the existing **C++ controller, plant and protection to WebAssembly**; it is not a separate JavaScript approximation or a prerecorded pose animation.

## Start in 30 seconds

1. [Open the lab](https://tinmanlab.github.io/actuator/) and move **Target angle**.
2. Press **Push +4 N·m / 120 ms**. Watch current rise and the link recover. Try **Meet a stop**, then **Trip driver**.
3. Recalculate [Powertrain](https://tinmanlab.github.io/actuator/electronics.html) for switched FOC, heat and torque–RPM maps. Open [Signals & losses](https://tinmanlab.github.io/actuator/physics.html). Inspect actual native gate edges, current decay, heating and signed power—not just the final pose.

## Choose your depth

| Your question | One place to answer it |
|---|---|
| Can I change switching, noise, heat and motor-map conditions? | [Recalculating C++/WASM Powertrain experiments](https://tinmanlab.github.io/actuator/electronics.html) |
| How does FOC work inside the loop? | [Interactive control pipeline and coordinate calculator](https://tinmanlab.github.io/actuator/control.html) |
| What do the six gates do? Why does current continue after shutdown? | [Native PWM, diode and current traces](https://tinmanlab.github.io/actuator/physics.html#pwm) |
| Where do heat, friction and efficiency enter? | [Reproducible thermal and power experiments](https://tinmanlab.github.io/actuator/physics.html#thermal) |
| Can I see actual 3D contact and impacts? | [Five C++ ↔ MuJoCo recordings](https://tinmanlab.github.io/actuator/#videos) |
| Which file or API should I change? | [Code map, compiled example and complete API](https://tinmanlab.github.io/actuator/api.html) |
| Which claims have a sound basis? | [Fidelity table](https://tinmanlab.github.io/actuator/physics.html#fidelity) · [14 annotated primary references](https://tinmanlab.github.io/actuator/references.html) |

## Run the code

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/qdd_live_example
./build/qdd_signal_lab results/my-signals
```

Use a **new output directory**. [Quickstart](docs/QUICKSTART.md) owns installation, native/MuJoCo commands and troubleshooting. [The experiment tutorial](docs/tutorials/signals.md) explains what to change and what to compare.

## Realistic mechanisms, explicit limits

The sinusoidal PMSM model includes R/L/back-EMF, current sensing and latency, two-inertia gearing, compliance/backlash, smooth friction, copper heating, R(T), DC-link regeneration and fault latches. Native switched experiments add gate events, dead time, conduction and diode paths.

**Live joint:** averaged inverter + 1-axis mechanics/compliant stop. **Powertrain:** switched electrical scope, prescribed-current heat, and averaged-inverter motor maps. **MuJoCo:** separate actual 3D mechanics/contact runs. **Not established:** a calibrated motor, manufactured PCB, STM32 worst-case execution time or hardware safety. Iron loss, switching-energy loss, magnetic saturation and winding hot spots are omitted; the displayed efficiency is a bounded synthetic calculation, not a product rating.

## Repository map

`src/` + `include/qdd/`: shared model/control · `web/`: live UI and lessons · `examples/`: runnable native/MuJoCo code · `tests/`: contracts · `tools/`: reproducible assets/evidence · `docs/`: setup and implementation decisions.

[Architecture](docs/ARCHITECTURE.md) · [Web build/deploy contract](docs/WEB_LAB.md) · [MIT license](LICENSE)
