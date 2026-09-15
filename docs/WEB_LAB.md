# Browser lab contract

## One implementation of motor physics

`web/live.cpp` is a thin interactive scheduler around the existing `Plant`, `Inverter`, `DcLink`, `Sensors`, `Drive`, and `FastTrip` components. It is compiled both as a native test and as WebAssembly. There is no separate JavaScript implementation of FOC, electromechanics, or contact.

The browser plant includes both mechanical inertias. It is **not** the external-MuJoCo mode. Its optional stop is a two-sided, periodic compliant contact envelope derived from the link/tip/stop geometry, not a general rigid-body collision solver. See `docs/CONTACT_AND_GUIDES.md` for its non-insertion and reverse-contact contract; the native CLI still has a separate one-sided stop fixture. It has no free falling object or general 3D collision solver. Those are separately identified MuJoCo films and local examples.

Timing: 50 μs controller/PWM period; ≤5 μs plant substeps, additionally split at protection and carrier-midpoint events; 1 kHz telemetry; the UI renders independently. The worker advances bounded blocks and never enlarges dt to meet wall-clock speed. Hidden tabs pause. No network requests send commands or data to hardware.

## State and interaction

Start/Pause preserves the physical state. **+1 ms** advances 20 PWM ticks while paused. Reset creates a new virtual instance (including power/thermal/fault state) and pauses. Algorithm changes also reset. Presets reset and start. Gate faults remain latched within one instance. This reset is intentionally not exposed as a real hardware-recovery API.

The public C facade is `web/live.h`; invalid commands are rejected, work per call is bounded, and an internal integration failure stops the instance. Native tests cover tracking, load displacement, contact, fault latching, validation, predictive control, and independence from worker block size.

WASM runs in an ordinary Web Worker without threads or SharedArrayBuffer, so GitHub Pages does not need cross-origin isolation headers. Three.js, OrbitControls, generated geometry, WASM, videos, and posters are hosted at the same origin. A module load failure is visible; there is no silent JavaScript physics fallback.

## Rebuild the entire site

With Emscripten, Node.js and Python installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
npm install --prefix web
python tools/build_site.py
python tools/build_signal_data.py
bash tools/build_wasm.sh
# Generate public videos from actual accepted MuJoCo runs:
python tools/verify_mujoco.py --backend osmesa --record --output results/site_mujoco
python tools/site_media.py --evidence results/site_mujoco
python -m http.server 8765 --directory _site
# Another terminal:
cd web && npx playwright install chromium
LAB_URL="http://127.0.0.1:8765/?paused=1" node smoke.cjs
node signals-smoke.cjs && node powertrain-smoke.cjs && node home-smoke.cjs && node dashboard-smoke.cjs
```

`tools/build_site.py` reuses the reviewed MJCF/OBJ recipe, exporting a compact scene representation for Three.js. Visual meshes never become a second source of mechanical mass. Camera controls and cutaway visibility do not change physics. No vendor CAD fidelity is implied.

## Verification and publication

`.github/workflows/pages.yml` builds and tests the native facade, compiles WASM, runs the actual MuJoCo acceptance suite, creates public videos, and drives the real browser with Playwright. It checks target changes, pause/step/reset, pulse load, contact reaction, fault latch, algorithm selection, CSV export, model inspection, all five video decoders, and narrow-screen overflow. A failed check prevents deployment.

Only `main` publishes to the `github-pages` environment. The test artifact includes screenshots, browser results, native results, generated assets and `build.json`. The deployed footer identifies source commit. Film metadata and SHA-256 hashes are in `media/evidence.json`. Generated media is a Pages build product, not an expiring API-only artifact link in the README.

GitHub README supports images and links, not this application's Worker, iframe or dependable video controls. Its linked screenshot/poster/animated GIF lead to the real embedded simulator and video gallery on Pages.

## Limits

No browser MuJoCo claim; no SPICE, firmware flashing, sensorless control, MTPA/field weakening, calibrated real motor, real-time MCU timing, or hardware-safety certification. Reduced telemetry is not a switching oscilloscope. The native and WASM floating-point implementations are tested functionally, not asserted bit-identical across targets. The mechanical and electrical model remains synthetic.

## Portable video playback

The public player offers VP9/WebM first, with the original H.264/MP4 as a second
source and direct links to both. Both encodings have matching decoded frame
counts and dimensions and separate SHA-256 hashes in the media manifest.
Playwright's open-source Chromium does not bundle every proprietary codec;
therefore video acceptance records advertised codec support, selected source,
media error state and decoded frame counts. It never treats a pending play promise
or a poster image as playback. All five recordings must decode before deployment.

References: [Playwright media codecs](https://playwright.dev/docs/browsers#media-codecs)
and [Chrome's nested-source promise caveat](https://developer.chrome.com/blog/play-returns-promise).
The browser-test dependency is pinned to Playwright 1.56.1 rather than the older
version affected by the browser-installer TLS advisory. No credential or runtime
permission is expanded by this dependency update.

## Learning-page ownership

README is the launch/value map, not another API manual. `control.html` owns the feedback pipeline and controller mathematics; `physics.html` owns signal traces, losses and the fidelity matrix; `api.html` owns interfaces and the compiled live example; `references.html` owns annotated external sources. `QUICKSTART.md` owns installation/troubleshooting and `tutorials/signals.md` owns experiment reproduction. Keep details in those owners and link rather than copying them.

The Pages build runs `qdd_signal_lab` through `tools/build_signal_data.py`. Generated CSV/JSON/ZIP stay in `_site/data`, not the source tree. Failure to generate native data blocks publication. The static signal page is a trace explorer, not a second real-time physics engine.


## Interactive powertrain and event media

`electronics.html` runs `web/experiments.cpp` in a separate Worker. The same host
API is compiled by CMake and Emscripten; it does not use an alternate JS plant.
The shared form recalculates switching, prescribed-current heat or a 28-point
motor-shaft map. See [the numerical/API contract](POWERTRAIN.md).

The joint autostarts when visible; use `?paused=1` for a paused entry. Reset and
algorithm changes still pause. Media selection uses native trace event times and
the renderer's explicit `playback_speed`; full MP4s are preserved in `media/full/`.
The generated `media/feature-tour.gif` concatenates all five event excerpts and
replaces the README still image. A loop restart replays an event, not reverse physics.

The Pages job additionally runs `web/powertrain-smoke.cjs` before deployment and
against the public URL. It checks autostart, recomputation, stale-result labeling,
torque/efficiency qualification, temperature changes, event loops and the GIF.


## First-screen live pipeline

The entry page is a single live joint, not a dashboard of independent solvers.
Five ordered, keyboard-operable stage buttons share one explanation region.
Selecting a stage is read-only; it only selects an existing result chart.
There is one scenario picker, one command editor per field and one selectable
joint-response chart. Additional current/duty/power scopes share the same run.
Inactive inputs are hidden; applicable feed-forward controls remain available
under Advanced, with a nonzero indicator. HTML and runtime command defaults are now consistently zero for velocity and
torque feed-forward. The previous HTML said 2 rad/s and 1 N·m, but the existing
startup JavaScript already overwrote them with zero. This corrects an inconsistent
initial representation and removes duplicate initialization; it does not claim
the previously deployed automatic-start trajectory was biased.

Independent switching/heat/maps, saved native traces and recorded MuJoCo examples
are explicitly separated below the live bench. Recordings are collapsed on entry;
`#videos` opens them. No iframe or second engine is started on the entry page.

`lab_signal(int field)` is an additive **read-only** browser adapter accessor.
The existing 11-command, 31-field `lab_get` ABI and CSV are unchanged. The Worker
sends a latest `signal` sidecar with the same timestamp as the last scene row in
each packet. Aligned histories for the instrument scopes use the same sidecar
values; no separate simulation state is created.
The UI rejects mismatched timestamps rather than combining different runs.
Before any control tick, field 1 is -1. Before initialization or for an invalid
field, the accessor returns NaN. Reads never advance time, RNG, filtering or control.

| Signal index | Meaning |
|---|---|
| 0 / 1 | Plant endpoint time / most recent controller tick time, seconds |
| 2 / 3 | Offset-corrected, sample-angle-aligned Id / Iq used by the last controller tick, A |
| 4 / 5 | Limited Vd / Vq commands from that tick, V |
| 6 / 7 | Current / encoder age at that tick, seconds (not age at UI rendering) |
| 8–10 | Raw, held ADC ia / ib / ic delivered to that tick, A |
| 11–13 | True phase ia / ib / ic at the plant endpoint, A |
| 14 | Controller current limit, A |
| 15 / 16 | Measured output angle / speed at the controller tick, rad / rad/s |
| 17 | Analog-filtered ia at plant endpoint, A |
| 18–24 | Command captured at the last tick: mode, position, velocity, torque, Iq, Kp, Kd (same units as `lab_set`) |
| 25 / 26 / 27 | Algorithm selector (0 PI, 1 predictive) / Id reference A / measured DC bus V |

The plant endpoint is 50 μs after the displayed controller tick. Held ADC samples
are older again; the UI labels those times separately. A command edited while
paused is pending, not retroactively applied to the displayed tick. Only Armed
state is described as active current regulation; gate-off does not imply zero
physical current. Frozen-duty gate reconstruction is labeled separately from
the averaged integration, as specified below.
The speed chart is plant speed only; position/torque references are hidden when
not applicable to the selected control mode.

`home-smoke.cjs` verifies default tracking, same-run timestamp binding, read-only
stage selection, pending commands, mode-specific inputs, fault/reset behavior,
collapsed media and responsive layouts on the built site and public Pages URL.
`live_readonly_signal_contract` verifies observational purity and unchanged native
trajectories; `homepage_pipeline_contract` guards ordering and zero feed-forward.

## Instrument dashboard (same-run observations, no raster concept overlay)

The main HTML uses a scoped dark instrument layout: phase/dq/duty histories,
3D hardware, PWM logic reconstruction, analog/ADC feedback, live temperature,
losses and an optional inline reference dyno. `dashboard.js` only plots native
observations. The existing 31-field CSV and 28-field signal API remain unchanged;
the Worker additionally sends their aligned 1 kHz sidecar histories, bounded to
8,000 rows. Rendering is capped near 10 Hz; the solver still uses its original
integration and control rates. Plotting or inspecting never advances the plant.

The pending-command problem was a flow-level paragraph repeatedly toggled by a
comparison between edited inputs and the previous tick's command. A reserved,
absolute-position status slot now prevents movement. Float command comparisons
use the MCU float representation. Sub-250 ms pending states while running are
not flashed as warnings; paused edits remain explicitly pending until Step or
Resume. The always-present chip describes the live command stream. Alert overlays
inside the viewport also preserve page geometry. No safety fault is hidden.

`lab_power(k)` is additive, read-only. Fields 1–11 are time means over the most
recent successful `lab_step(n)` call (one millisecond in browser telemetry).
0: endpoint seconds; 1: inverter DC input W; 2: motor terminal AC W; 3: inverter
conduction/dead-time proxy W; 4: copper W; 5: rotor+output friction W; 6: gear
relative-motion dissipation W; 7: external load work rate W; 8: stop work rate W;
9: inverter DC current A; 10: electrical+kinetic+elastic stored-energy rate W;
11: remaining power-balance residual W; 12: endpoint case °C; 13: phase resistance Ω;
14: stored energy J; 15: brake resistor W. Copper uses the bridge evaluation current
and resistance at the step start; mechanical work uses endpoint-average velocity.
These are diagnostics of the existing integration, not an exact energy-conserving
solver. The residual is displayed and must not be relabeled as dissipated heat.

Live efficiency is a 200 ms inverter-DC-to-external-load ratio, shown only with
at least 180 valid 1 kHz samples, positive input/output, no fault, ratio ≤1, and
stored-energy rate below 5% of max(1 W, |input|). Otherwise it is unavailable.
Stop work is separate and is not credited as useful output. Unmodeled iron and
switching-energy losses are not populated. The live torque/speed trail uses
gear-input torque (= output coupling torque / 6) and motor RPM. It is not a
rated torque envelope. The optional 28-point dyno reuses `experiment-worker.js`
with explicit independent default parameters and the existing motor-shaft
qualification; it neither inherits nor controls the joint. No second solver
starts until requested. Live and reference efficiency boundaries are labeled.

`lab_pwm(phase_seconds,k)` reconstructs **one frozen-duty PWM period** using the
native `Inverter` class in switched mode, with the live gate-enable latch and
current duty. It never integrates current or changes the averaged live model.
Fields 0–2 are three comparator requests, 3–8 AH/AL/BH/BL/CH/CL after native
150 ns dead time, and 9 is the carrier. This is not Vgs, an oscilloscope capture,
or live switching ripple. The 3PWM/6PWM selector changes which signals the
interface diagram exposes, not motor phase count or plant fidelity. Both modes
share the same three half bridges. See SimpleFOC driver configuration and current
sensing references already linked in the reference guide. ADC senses current;
DAC is not a power-current smoothing stage.

Geometry now includes an illustrative 48 V source with distinct DC pair, U/V/W
harness and encoder return. Phase leads pass a real notch in the fixed rear cap
and the mount's annular bore. Sampled centerline-plus-radius clearance checks
cover column, foot, bench, PCB, DC-source box and mount ring. These checks are
not a complete CAD interference certification. Original dynamic mass, collider
and hinge definitions are preserved; added supply/cables are zero-mass,
non-contact decoration. No fabrication-ready circuit or vendor CAD is implied.

`dashboard-smoke.cjs` verifies numerical histories, six gate nodes, read-only
interface inspection, paused and continuously edited commands without viewport
shift, native inline map, asset presence and responsive screen captures. All
pre-existing native and browser acceptance suites remain enabled.
