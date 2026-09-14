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
node signals-smoke.cjs && node powertrain-smoke.cjs
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
