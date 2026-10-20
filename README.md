# Stimulus

A C++17 / raylib application for creating and presenting visual stimulus sequences,
with native Linux and browser (WebAssembly) builds.

## Features

- Fixation cross, uniformly distributed points in an annulus, and independently
  sampled color words (Stroop-like stimuli; not a complete standardized Stroop task).
- Editable duration, repetitions, update rate, seed and stimulus-specific parameters.
- Ordered experiment sequences with independent configuration copies.
- Monotonic timestamps in **seconds**, response keys, trial boundaries and frame counts.
- JSON libraries and session reports, plus CSV response export.
- A single non-blocking frame loop shared by native and Emscripten builds.

## Native Linux

Install a C++17 compiler, CMake 3.22+, Git and OpenGL/X11 development packages.
On Ubuntu/Debian:

```sh
sudo apt-get install build-essential cmake git libgl1-mesa-dev libx11-dev \
  libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/stimulus
```

CMake fetches **raylib 5.5** and **JsonCpp 1.9.6** at pinned source commits. Raygui
is no longer required: the small editor uses raylib directly. The old precompiled
`s` executable has been removed; build outputs are not committed. Other native
platforms are not yet covered by CI.

Headless core tests (no graphics dependencies):

```sh
cmake -S . -B build-core -DSTIMULUS_BUILD_APP=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

## WebAssembly

Install and activate [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
**3.1.64**, the version used by CI. With its environment active:

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel
python3 -m http.server 8000 --directory build-web/web
```

Open <http://localhost:8000>. Serve the entire `build-web/web` directory; opening
`index.html` via `file://` does not work. URLs are relative, including under the
GitHub Pages `/stimulus/` project path. No server backend or COOP/COEP headers are required.

Browser regression test, with the local server running:

```sh
npm install --no-save --package-lock=false playwright@1.51.1
npx playwright install chromium
node tests/web_smoke.cjs
```

## GitHub Pages

The workflow builds native and WebAssembly targets and runs core regression tests
and a Chromium smoke test. PRs only build/test. Successful builds on `main` deploy
through the official Pages artifact workflow.

**One-time repository setup:** in **Settings → Pages → Build and deployment**, set
**Source = GitHub Actions**. Merge this change into `main` or run **Build, test and
deploy** manually on `main`. The expected URL is
<https://ze-gois.github.io/stimulus/>; it becomes available only after a successful
deployment. The Pages setting is a repository administration step, not set by this code.

A `stimulus-web` artifact is available from CI even before Pages is enabled.

## Usage and data

1. Select a library stimulus, or choose **New** / **Edit** and adjust the fields.
2. **Apply** updates the in-memory library. **Add >** appends an independent copy to
   the experiment. Select a sequence item to **Remove** it or **Move up**.
3. **Save library** saves both lists. Changing a library item does not change copies
   already in the sequence: remove and re-add an entry to use the new settings.
4. **Start** runs the sequence. Keyboard presses become responses. **Escape** or
   window/tab focus loss aborts the session and opens the report.
5. Export session **JSON** and/or response **CSV** before starting a new session.
   Reports are held in memory until exported; refreshing/closing discards them.

Native saves use `files/library.json` relative to the working directory. Exports
are written to `files/exports/`. Native **Import JSON** reads `files/import.json`.
When no new library exists, legacy `files/stimuli/*.json` files are read in sorted
filename order, without deleting originals. Old `center_Y`, `outter_radius`,
`repetition` and `repetitionw` fields are accepted; saves use canonical names.
Invalid configurations are rejected instead of silently clamped.

The browser uses `localStorage` (`stimulus.library.v1`) on the current origin.
**Import JSON** above the canvas accepts a library or a legacy single stimulus;
import replaces the current in-memory library, and **Save library** persists it.
**Export library** downloads a portable JSON backup. Browser storage can be cleared
or unavailable in private modes; export a backup for data you need to retain.
No experimental data is uploaded by the application.

### Timing and randomness

Duration is measured using raylib's monotonic `GetTime()`, not a frame count.
Each repetition receives its full duration starting at its actual software frame
boundary; a late frame does not skip subsequent stimuli. `repetitions = 1` means
exactly one presentation. JSON records actual software start/end times and frame
counts. CSV indices (`trial`, `sequence_index`, `repetition`) are zero-based;
`key` uses raylib keyboard codes. Escape is a control action, not a response.

The requested update rate controls random-circle updates. The browser renders
through `requestAnimationFrame`, bounded by monitor/browser scheduling; requesting
240 Hz does not guarantee 240 displayed frames. Late callbacks regenerate one
pattern without replaying unseen frames. Words stay fixed within each repetition.
Random streams use the seed, sequence index and repetition index; reproducibility
of continuously changing stimuli also requires the same callback schedule and
standard-library implementation. The report stores configuration, not every dot
position or measured screen luminance.

Timestamps are **software observations, not measured photon onset**. Rendering,
input polling, the display and operating system add latency. Focus loss aborts
rather than silently resuming an interrupted experiment. Use a photodiode and
hardware synchronization when physical stimulus timing matters.

## Layout

- `src/model.*`: validated configuration, random geometry and session state machine.
- `src/storage.*`: JSON compatibility/validation and report/CSV serialization.
- `src/platform.*`: native files / browser storage and download bridge.
- `src/app.*`, `src/main.cpp`: interface, renderer and platform frame loop.
- `web/shell.html`: responsive canvas host and JSON import.
- `tests/`: core regressions and browser integration smoke test.
- `.github/workflows/build.yml`: builds, tests, artifacts and Pages deployment.
