# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DynaCharting is a high-performance real-time charting engine intended as an embeddable library for internal use. The C++ core does the heavy lifting for both data processing and rendering. The TypeScript/WebGL2 frontend served as a prototype to prove out concepts (scene graph, pipelines, data ingestion); it has been **fully retired** (ENC-508) now that the C++ core owns rendering. The browser/WASM path is `@repo/dc-wasm` (C++ core compiled to WebAssembly, rendering via WebGPU).

> **Read [`LIMITATIONS.md`](LIMITATIONS.md) before designing around a constraint.** It is the
> repo-local, dated log of what this engine cannot do, what it does differently than you
> expect, and what is built but unreachable — each entry with a paste-able `Re-check` command
> and the commit it was last verified at. It also records what *stopped* being true (§R) and
> which confidently-held beliefs were wrong (§C). **Keeping it true is part of the definition
> of done** — see [Workflow](#workflow).
>
> **Adding an entry? Its id is `DC-L-<your ENC ticket number>` — `DC-L-1277`, with the hyphen
> — never the next free `DC-Lnn` (ENC-1277).** The sequential space is **closed at `DC-L18`**.
> Author-picked numbers are racy by construction and the window is the whole life of a branch,
> not the moment you check: they collided three times in two days (`DC-L12`, `DC-L14` three
> ways, `DC-L17`) and **every one of those authors checked first**, one of them across every
> `enc-12*` remote branch. Twice there was **no merge conflict at all** — git auto-merged the
> two entries into different parts of the file, leaving two identical headings with no marker
> and no error. Linear allocates the ticket number, so there is nothing left to race for. The
> existing eighteen were deliberately **not** renumbered: they are citation targets (234
> citations, 35 files, two repos, plus merged commit messages and Linear comments that cannot
> be rewritten), and nine of them arrived in one commit with no ticket of their own, so
> renumbering them by ticket would itself collide nine ways. Merged entries cannot collide;
> only future ones can. The hyphen is load-bearing — without it `grep DC-L12` matches
> `DC-L1277`. Enforced by `bash scripts/check-limitation-ids.sh`, which `pnpm test` runs; it
> also fails when an id that existed on `origin/main` has vanished, the `--ours` conflict
> resolution that silently ate a whole entry once. Full reasoning: `LIMITATIONS.md` §H device 7.

**Current milestone:** WebGPU/Dawn is the C++ renderer (`dc_gpu`). The original OpenGL backend has been removed (ENC-501); the full pipeline set renders headless through Dawn with offscreen readback. The TypeScript/WebGL2 prototype (`engine-host`/`chart-controller`/`hello-engine`) has been retired (ENC-508) — `@repo/dc-wasm` is the browser path. Windowed/on-screen presentation is next (ENC-497).

## Repository Layout

- **`core/`** — C++17 static libraries (`dc` and `dc_gpu`). Scene graph, command processor, resource registry, pipeline catalog, and the WebGPU/Dawn rendering backend. This is where most new work happens. Built with CMake.
  - `dc` — Pure C++ core (no graphics-API deps). Scene graph, commands, pipelines, plus the backend-agnostic CPU-side buffer base `CpuBufferStore` (`dc/render/`).
  - `dc_gpu` — WebGPU/Dawn rendering backend — **THE renderer**. Built only with `-DDC_FETCH_DAWN=ON` (building Dawn is heavy); links `dc` and `dawn::webgpu_dawn`. Contains `DawnDevice`, `DawnSceneRenderer`, and the 10 per-pipeline Dawn backends. See the WebGPU/Dawn section under Build & Development Commands.
  - The OpenGL backend (`dc_gl`) and its deps (GLAD, OSMesa, GLFW) were **removed** in the WebGPU/Dawn migration (ENC-501). On-screen/windowed presentation on Dawn is a separate ticket (ENC-497).
- **`packages/dc-wasm/`** — WASM + WebGPU browser package (`@repo/dc-wasm`). **THE browser/WASM path.** Compiles the C++ `dc` core to WebAssembly and renders via WebGPU, exposing an `EngineHost` TS surface. This is what customer-layer consumes.
  - The original TypeScript/WebGL2 prototype (`@repo/engine-host`, `@repo/chart-controller`, and the `apps/demos/hello-engine` demo) is **RETIRED** (ENC-508). It proved out the scene graph, pipelines, glyph atlas, and data-ingestion concepts; those are now owned by the C++ core (`dc`/`dc_gpu`) and surfaced to the browser through `@repo/dc-wasm`.
- **`apps/live-viewer/`** — Standalone live-stream viewer (`@repo/live-viewer`). Independent of the renderer packages; talks to a headless render server.

## Build & Development Commands

### TypeScript (pnpm workspace)

```bash
pnpm install                                        # install all workspace deps
pnpm --filter @repo/dc-wasm build                   # type-check the browser package (does NOT rebuild the wasm — see below)
pnpm --filter @repo/live-viewer build               # build the live-stream viewer
```

### C++ Core (CMake)

```bash
git submodule update --init third_party/rapidjson    # once: vendored RapidJSON
cmake -B build                                       # configure
cmake --build build                                  # build library + logic tests
ctest --test-dir build                               # run the logic tests
ctest --test-dir build -R dc_d1_1_smoke              # run a single test by name
```

The **default** build (no `-DDC_FETCH_DAWN`) builds `dc` + the pure-logic tests only — no renderer, fast, and needs no graphics API. To get the renderer + render/golden tests, opt into Dawn (see below).

> **A green default `ctest` proves nothing about the renderer (LIMITATIONS.md DC-L01).** The
> default configure registers **196** of the repo's **243** tests; the other **47** — every
> Dawn render and golden-parity test, and the tier-0 check below — plus `dc_gpu`,
> `dc_json_host` and the four headless servers are excluded at *configure* time, so nothing
> reports them as missing. "196/196 passed" is compatible with the renderer being completely
> broken. Verify with
> `grep -c '^add_test(' build/core/CTestTestfile.cmake` (196) against
> `grep -cE '^\s*add_test\(' core/CMakeLists.txt` (243). The pair moves as tests are added
> (188/231 before ENC-984, 189/232 before ENC-995, 190/233 before ENC-1249, 190/236 before
> ENC-1257, 191/238 before ENC-1251 and before ENC-1253, 193/240 measured at `f907f93` under
> ENC-1277, 193/240 before ENC-1265); the **47-test gap** is the number that matters — it has
> not moved through any of them.
>
> *(ENC-1251 and ENC-1253 each amended the parenthetical above on their own branch; git
> auto-merged the two versions into a duplicated, orphaned copy of this blockquote with no
> conflict and no marker. ENC-1277 folded them back together — it is the same silent
> auto-merge that put two `DC-L14` headings in `LIMITATIONS.md`, one file over.)*

CMake options: `DC_BUILD_TESTS` (default ON), `DC_WARNINGS_AS_ERRORS` (default OFF), `DC_FETCH_DAWN` (default OFF — see below).

**Note:** Only RapidJSON is needed under `./third_party` for the default build. It is a git
submodule — run `git submodule update --init third_party/rapidjson` after cloning, or the
`dc` target fails to compile. There are no longer any GLAD / OSMesa / GLFW dependencies —
the GL backend was removed.

**Do not pass a relative `-DTHIRD_PARTY_ROOT`.** CMake resolves a relative value against the
*build* directory, so `-DTHIRD_PARTY_ROOT=./third_party` becomes `build/third_party` and the
build fails on a missing `stb_truetype.h`. The default is already
`${CMAKE_SOURCE_DIR}/third_party`, so just omit the flag; pass an **absolute** path only if
your third-party tree lives elsewhere (ENC-876).

### WASM / browser build (`@repo/dc-wasm`)

The browser artifacts (`packages/dc-wasm/wasm/dc_engine_host.{js,wasm}`) are
**committed**, and they are rebuilt from the C++ core by one command:

```bash
source ~/emsdk/emsdk_env.sh                          # once per shell
bash packages/dc-wasm/scripts/build-wasm.sh
node packages/dc-wasm/scripts/validate-node.mjs      # functional check of the result
```

**One-time emsdk provisioning.** No sudo; everything lands under `$HOME`:

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install 6.0.9 && ./emsdk activate 6.0.9
source ~/emsdk/emsdk_env.sh
```

That is the whole prerequisite list. `build-wasm.sh` provisions RapidJSON itself
(`git worktree add` does **not** populate submodules, which is why a worktree
that looks clean still failed to build), and Ninja and a C++20 clang come from
emsdk. The `emdawnwebgpu` port — the Dawn/WebGPU implementation — is downloaded
by Emscripten on first link and cached, so the first build is slow and later
ones are not. If the command above fails on a clean checkout with emsdk
sourced, that is a bug in the script, not in your setup.

**The emsdk version is pinned (6.0.9) and that matters.** Because the artifacts
are committed, an unpinned toolchain makes a rebuild produce a diff for reasons
unrelated to any source change — which is how a toolchain bump rides into an
unrelated PR unnoticed. The pin also fixes the `emdawnwebgpu` port version,
since Emscripten selects it. `build-wasm.sh` warns (but does not fail) if your
`emcc` differs.

With the pin, the build is **byte-reproducible**, and reproducible across
*checkout locations* — verified by building the same commit from two different
directories and comparing hashes. The second part needs
`-ffile-prefix-map` (passed by `build-wasm.sh`): without it, `__FILE__` bakes
the absolute source path into the module, so two developers with different
checkout paths produce different bytes from identical source. With committed
artifacts that would mean a spurious diff on every rebuild.

So a rebuild that produces no diff is the expected outcome, and a diff means
something real changed:

```bash
sha256sum packages/dc-wasm/wasm/dc_engine_host.wasm   # before
bash packages/dc-wasm/scripts/build-wasm.sh
git diff --stat -- packages/dc-wasm/wasm/            # expect: no change
```

**Bumping emsdk** is deliberate: change `EMSDK_VERSION` in `build-wasm.sh`,
rebuild, and commit the artifact churn on its own rather than folded into a
source change.

**What is *not* in the module.** The browser surface is the single Embind
`DcEngineHost` class in `core/wasm/dc_engine_host.cpp`. The C++ `dc::*Recipe`
family (`core/src/recipe/`) is **not bound**, so it is dead-code-eliminated and
`strings dc_engine_host.wasm | grep -ci recipe` is `0` — before and after this
build. Exposing recipes to the browser is ENC-990; provisioning the toolchain
(ENC-989) is what unblocks it, and does not by itself change what the module
exports.

> **Unbound means dead-stripped, and that is the whole mechanism (ENC-984).** A
> tested C++ function that nothing in `dc_engine_host.cpp` names is simply not in
> the artifact — `serializeSceneDocument` had been round-trip tested since D77 and
> `strings dc_engine_host.wasm | grep -ci sceneDocument` was **0**. Adding one
> `.function(...)` line to `EMSCRIPTEN_BINDINGS` is what makes it real, and the
> artifact is committed, so **the export does not exist until you rebuild the wasm
> and commit it**. Check an export the same way: `strings` for the name, then call
> it from `scripts/validate-node.mjs`. Binding one function pulled in its
> transitive code and grew the wasm by ~209 KB (2.72 MB → 2.93 MB) — expect that,
> and do not read it as unrelated churn.

**Scene export (ENC-984).** `getSceneDocument(compact)` returns the live scene as
`SceneDocument` JSON — a **copied string**, not a `typed_memory_view` like
`framebuffer()`/`getBufferBytes()`, so it is safe to hold across renders. Buffer
**bytes** are deliberately not inlined: the document carries each buffer's
`byteLength` and you read the contents with `getBufferBytes(id)`. Three schema
sections always come back empty (`viewports`, `textOverlay`, `bindings`) because
the `Scene` has no representation for them — **LIMITATIONS.md DC-L10**, which is
the reason "serialize the scene" is not by itself view-state persistence.

### WebGPU / Dawn backend (`dc_gpu`)

`dc_gpu` is the renderer, but Dawn is **OFF by default** because building it from source is heavy. The default `dc` build + logic tests are unaffected unless you opt in.

Enable Dawn (fetches and builds it from source via CMake `FetchContent`). Use Ninja:

```bash
cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
cmake --build build-dawn -j$(nproc)
# Dawn render tests need a real Vulkan ICD; on a headless box use the lavapipe fallback:
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ctest --test-dir build-dawn -j$(nproc)
```

The Dawn build adds `dc_gpu`, the `dc_json_host` embedding host, the headless render servers (`dc_showcase_server`, `dc_live_server`, `dc_dashboard_server`, `dc_gallery`), the per-pipeline Dawn render tests, and the Dawn-golden parity tests.

#### The tier-0 check — does the chart depict its data? (ENC-1249)

```bash
bash scripts/tier0.sh                 # one command; runs the check AND its two negative controls
bash scripts/tier0.sh <build-dir>     # default build dir: build-dawn
```

`specs/2026-09-19-chart-quality-bar/SPEC.md` **D1** defines tier 0 ("Truthful") as *the mark
depicts the data*, and its falsifiable check as: render a **known-answer synthetic series** and
assert on pixels. That check is `core/tests/dc_enc1249_tier0_truthful.cpp` — a monotonic ramp
through the real `LineRecipe` + `dc::LinearScale`, a single candle with hand-computed extents
through the real `CandleRecipe`, and (ENC-1250) a baseline **area** that must fill from its
baseline up to the value, on the real `instancedRect@1` rect4 layout and the real
`price-line-area` transform — all rendered by `DawnSceneRenderer`.

Four things about it are deliberate and easy to get wrong if you extend it:

- **Synthetic data is correct HERE and only here.** SPEC **D2** bans synthetic feeds for the
  *reference chart* and permits them for tier-0 assertions, because a known answer is the entire
  point. Do not read this file as licence to shim a feed anywhere else.
- **It asserts on the PRESENTED raster** — the readback plus the row flip
  `EngineHost.blitFramebuffer` applies on every browser frame (**DC-L05**). The raw readback is
  vertically mirrored; asserting on it would enshrine the mirror and "prove" that a rising series
  falls. See LIMITATIONS.md **C5**.
- **It ships its own negative controls, and they are registered tests.**
  `--invert-data` (a descending ramp; candle body/wick extents swapped; the area's baseline moved
  *above* the series) and `--invert-render` (skip the DC-L05 flip) are `ctest` cases with
  `WILL_FAIL TRUE`, so every run of the suite re-demonstrates that the check *can* fail. A check
  never seen to fail is not a check. Note the area's control moves the **baseline**, not `y0`/`y1`
  — the shader mixes `y0..y1`, so swapping them renders identically and would not be a control
  at all.
- **It does not skip gracefully.** No Dawn adapter is exit **3** ("CANNOT RUN"), never 0 —
  DC-L01's lesson is that a skip which looks like a pass is how a green run came to mean nothing.

#### The plot box — the frame data is fitted into (ENC-1256)

`packages/dc-wasm/src/chart/plotbox.ts` is this engine's **only** plot-box / margin concept.
Before it, geometry was projected into the whole of clip space and nothing reserved a band for
axis furniture — which is why the live chart measured 66.4% dead margin with seven candles
sharing one clipped top row (SPEC §1.0).

```ts
import { frameSeries, checkTier2Framing } from '@repo/dc-wasm';       // or '@repo/dc-wasm/chart'
const framed = frameSeries(tracker.domain(), { width, height });      // tracker = ENC-1252 DomainTracker
host.applyControl({ cmd: 'setPaneRegion', id: PANE, ...framed.paneRegion });
host.applyControl({ cmd: 'setTransform', id: TRANSFORM, ...framed.transform! });
checkTier2Framing(framed.metrics!);   // { pass, failures } against SPEC D1's tier-2 measures
```

Five things to know before building on it, each stated in full in the module header:

- **`plotBox()` THROWS, so a caller that does not own its canvas must call `tryPlotBox`
  (ENC-1313, LIMITATIONS.md DC-L-1313).** The gutters are absolute pixels, so any canvas
  narrower than `left + right` (80px by default) has no box at all, and `plotBox()` refuses it
  by throwing `PlotBoxError` — correct for a builder, fatal for a React effect. **A mounting
  canvas is 1x1**: `ShowcaseEngine.sizeCanvas` publishes `Math.max(1, …)` for one commit before
  layout. That throw, escaping `useEngineAxis`'s passive effect into a tree with no error
  boundary, **unmounted the entire app on a deep link to 11 of the 22 showcase views** (measured
  3/3 cold loads each at `1e125e3`). Use `tryPlotBox(canvas, insets)` →
  `{ fits: true; box } | { fits: false; reason; detail }` anywhere the size is handed to you, and
  make the refusal visible — `engineAxisSpec` returns a named `refusal` and both
  `window.__dcEngineAxis` and the overlay's `data-dc-engine-axis-refusal` carry it. Re-check:
  `specs/2026-09-19-chart-quality-bar/harness/deeplink-crash.mjs` (0 of 22, exit 0).
  **The reference chart could not have caught it** — `candles-aapl` is a `timestamp` view, whose
  axis resolves after layout, so it is structurally in the surviving half. That is SPEC §5 Q4's
  negative transfer, and the reason "we verified it on the reference" is not coverage.
- **Gutters are CSS pixels, the box is clip units.** A tick label is 11px tall at any chart
  size. `plotBox()` is the only conversion; `pxSpanToClipX/Y` are SPAN helpers and are NOT
  `text.ts`'s same-named POSITION helpers.
- **The domain is ink extent, not mark centres** — `DomainTracker` folds candle6's `halfWidth`.
  That is what makes "no geometry touches the frame edge" follow from a positive inset, and it
  is why `paddingFrac` defaults to 0.
- **The box is also the data pane's `PaneRegion`.** `paneRegionFor(box)` derives it, so the
  scissor and the projection cannot drift. Axis furniture drawn in the gutters therefore needs
  a pane with a wider region (`FULL_CLIP_REGION`), or it is scissored away silently.
- **The DATA is framed by it on the showcase, and only there (ENC-1273).**
  `apps/showcase/src/views/useViewSwitch.ts` fits the ENC-1252 measured domain into the box and
  applies **both** halves `frameSeries` returns; `ChromeOverlay` then maps its ticks through
  that same fitted transform and that same box, so the furniture and the geometry cannot
  describe two frames. That retired DC-L14. What it does NOT cover is **DC-L-1273**: a stacked
  multi-pane view is *refused* (`apps/showcase/src/views/framing.ts` counts `createPane` — one
  box cannot lay out two panes), which includes `candle-overlays`, the view `/` renders; and
  `customer-layer`, the surface SPEC §1.0 measured, has adopted none of it. So a green
  `plotbox.test.ts` still is not the product being framed — score the raster (SPEC D10).
- **Two things that will bite the next adopter**, both found by ENC-1273 rather than reasoned
  about. `useReplay`'s `xAnchor` writes its own `setTransform` on the first record of every
  replay pass, so it must not stay armed on a framed view or it overwrites the fit once per
  loop. And the fit must be recomputed against the **backing-store** canvas size — the same one
  `ChromeOverlay` hands the axis — or the two `plotBox()` calls differ by the device-pixel
  ratio and the furniture lands near, but not on, the frame.

#### The axis — drawn by the engine (ENC-1253)

`packages/dc-wasm/src/chart/axis.ts` is where gridlines, tick marks, the spine and the tick
labels come from. They are **engine geometry, not DOM**: `lineAA@1` rect4 segments authored
directly in clip space, and `textSDF@1` glyph runs, into the same scene and the same canvas as
the data (SPEC **D7**, **D6**/**D10**).

```ts
import { EngineAxis, createHostMeasurer, planAxis, darkAxisTheme, frameSeries } from '@repo/dc-wasm';
const framed = frameSeries(tracker.domain(), canvas);          // ENC-1256
const axis = new EngineAxis(host, createIdAllocator(900000));
axis.sync({ box: framed.box, canvas, transform: framed.transform!,
            y: { ticks: priceTicks }, x: { ticks: timeTicks },   // ENC-1254 shapes
            theme: darkAxisTheme,
            measurer: createHostMeasurer(host, canvas)! });
```

Five things to know before extending it — each one is a way the axis fails **silently**:

- **The furniture needs its OWN pane, at `FULL_CLIP_REGION`.** The data pane's region *is* the
  plot box (plotbox.ts note 7), applied as a scissor, and ticks/labels live in the gutters
  outside it. Draw them in the data pane and they are clipped away with nothing rejected.
- **…and that pane must be created AFTER every pane it sits on top of, and re-created whenever
  those are — LIMITATIONS.md DC-L18.** A pane's clear colour is a drawn quad walked in scene
  order, so a newer pane *erases* an older one's pixels. The showcase re-applies its manifest on
  every replay loop; `useEngineAxis` watches `useViewSwitch`'s `sceneEpoch` and rebuilds.
- **Text is MEASURED, never estimated.** `DcEngineHost.measureText(text, fontSize, xScale)`
  (ENC-1253) runs the same `dc::layoutText` loop `setTextGeometry` runs, so a label's reported
  box is where its glyphs land. `fontSize` is the ascent-to-descent height **in clip units**
  (`fontSizeForPx(px, canvas)`), and `xScale` must be `canvas.height / canvas.width` or every
  string renders stretched by the canvas's aspect ratio — use `setTextGeometryX`.
- **`setTextGeometry` and `measureText` return -1 while a render is in flight**, and the engine
  renders from its own rAF loop, so a naive sync in a React effect loses every label. Probe,
  then retry on a macrotask (`useEngineAxis`), and never write half an axis: a fresh gridline
  beside a stale number is the disagreement this whole project is about.
- **Colours come from a `Theme`** — `packages/dc-wasm/src/chart/theme.ts` mirrors `dc::Theme`'s
  axis knobs and `theme.test.ts` parses the C++ and fails if the two drift. The default is
  `darkTheme()`, chosen by measurement: `midnightTheme()`'s `tickColor` is 2.46:1 on the black
  the render target clears to, under D11's 3:1 floor. `checkD11AxisBands()` is that check.

Score it the way the project requires — a **canvas-only** capture, a hardware adapter, and a
scene dumped by the renderer rather than typed:

```bash
# showcase dev server on its own port; Chrome with all three hardware flags (harness/README.md)
node specs/2026-09-19-chart-quality-bar/harness/shoot-live.mjs \
  --port <cdp> --url 'http://localhost:<port>/?svgAxis=0#/view/candles-aapl' \
  --canvas 'canvas.engine-canvas' --dwell 22000 --out /tmp/<ENC>-shot.png
node apps/showcase/tools/axis-scene.mjs --port <cdp> --view candles-aapl \
  --raster /tmp/<ENC>-shot.png --out /tmp/<ENC>-scene.json
python3 specs/2026-09-19-chart-quality-bar/harness/score.py \
  --raster /tmp/<ENC>-shot.png --scene /tmp/<ENC>-scene.json --diagnose
```

`--diagnose` is not optional today: tier 0 is UNPROVEN on every build without a
`-DDC_FETCH_DAWN=ON` `scripts/tier0.sh` artifact (DC-L01), and an UNPROVEN tier 0 marks every
tier above it NOT SCORED.

> **The committed stills are no longer upside down (ENC-1288 recaptured them; DC-L15 is
> RETIRED) — and you still do not score one.** Until 2026-09-20 every PNG in
> `apps/showcase/stills/` was captured at `537c995` (2026-06-11) while the `EngineHost` blit
> flip landed at `d6b5acd` (2026-06-21), so all of them were vertically mirrored. A mirrored
> random walk still looks like a random walk and the axis numbers were a DOM overlay that
> flipped with it, so nothing in the frame contradicted the mirror — which is how
> `price-line-area` was read as "fills on the wrong side of the line" for three months
> (**§C6**). They are now canvas-only frames taken on a named hardware adapter
> (`apps/showcase/stills/capture-manifest.json`), regenerated by
> `node apps/showcase/tools/recapture-stills.mjs`.
>
> **The scoring rule is unchanged, and it never rested on the mirror**: a tier-1 judgement is
> made on a fresh canvas-only capture scored against the scene the page declared in the same
> eval (SPEC D10). A still is one frame of a loop and the loop moves. To check one's
> orientation: `python3 apps/showcase/tools/still-orientation.py <view>` (exit 0 upright, 1
> mirrored; `--still <path>` rules on a file that is not the committed one).

> **`dc_json_host --png` captures contain no text (ENC-992).** A chart's
> `textOverlay` labels are not rasterized by the engine — they are emitted as a
> `TEXT` protocol message for the browser client to composite, so a one-shot PNG
> is the chart minus every title, axis label and legend. The GPU text pipeline
> (`textSDF@1`) is also unregistered in this host (it wires no `GlyphAtlas`), so
> `textSDF@1` draw items are silently skipped there too. Don't read a textless
> capture as a rendering bug. `dc_json_host --help`, the comment at the `--png`
> early return in `core/src/host/JsonHost.cpp`, and CHART_AUTHORING.md §15 all
> spell this out. Full mechanism: LIMITATIONS.md **DC-L02**.
>
> **`--png` is also pathologically slow (LIMITATIONS.md DC-L03).** It does one
> `readPixel()` GPU round trip *per pixel* — 540,000 of them at the default
> 900×600 — so a full-size capture exceeds 300 seconds. Being fixed under
> ENC-1093; re-check DC-L03 before planning around it.

- **Pinned Dawn revision:** commit `58263faefe3c52fac4656825c6d55f85ee3c7536` — the immutable tip of branch `chromium/7880` as of **2026-06-09**. We pin an explicit commit hash (never a moving branch) for reproducibility. Update this hash deliberately when bumping Dawn.
- **Source:** `https://dawn.googlesource.com/dawn`. Dawn's own dependencies are fetched with its `fetch_dawn_dependencies.py` helper (`DAWN_FETCH_DEPENDENCIES=ON`), so `depot_tools` is **not** required.
- **Build cost (heads-up):** build-from-source is **slow** — the first configure clones ~3-4 GB of Dawn + its third-party deps, and a full compile takes **30-60+ minutes** and needs `python3` and `ninja`. Subsequent incremental builds are fast. The full Dawn build is validated in CI (ENC-499).
- **Lean build:** Dawn samples, tests, benchmarks, fuzzers, node bindings, install rules, and Tint command-line tools/tests are all disabled. Dawn is built as a single monolithic static library.
- **Linked target:** `dc_gpu` links the Dawn monolithic WebGPU target `dawn::webgpu_dawn` (alias of `webgpu_dawn`) plus `dc`. When `DC_FETCH_DAWN=OFF` (or Dawn is unavailable), `DC_HAS_DAWN` is FALSE and `dc_gpu` (and everything that needs it — the host, the servers, the render tests) is gracefully skipped; the default `dc` + logic-test build is unaffected.
- `dc_gpu` is the full WebGPU/Dawn renderer: `DawnDevice` (offscreen target + readback), `DawnSceneRenderer` (the scene-walk mirror of the old GL `Renderer::render`), and the 10 per-pipeline backends (triSolid/triGradient/triAA/line2d/lineAA/points/instancedRect/instancedCandle/textSDF/texturedQuad) + picking.

#### Windowed (on-screen) presentation — `DC_DAWN_WINDOWED` (ENC-497)

The default Dawn build is **headless** (offscreen render + readback). On-screen presentation is an **additive, opt-in** build path enabled with `-DDC_DAWN_WINDOWED=ON` (default OFF, so the headless build — the 169 tests + the WASM/browser targets — stays lean and needs no windowing system).

```bash
cmake -B build-win -G Ninja \
  -DDC_FETCH_DAWN=ON -DDC_DAWN_WINDOWED=ON \
  -DFETCHCONTENT_SOURCE_DIR_DAWN=~/dawn-src
cmake --build build-win --target dc_gpu dc_dawn_window_demo
# Run on the display (force lavapipe if no HW adapter):
DISPLAY=:0 ./build-win/core/dc_dawn_window_demo --frames 30 --out /tmp/dawn_window.png
```

What `DC_DAWN_WINDOWED=ON` changes:
- Re-enables Dawn's **X11 Vulkan surface** (`DAWN_USE_X11=ON`). Wayland and Dawn's *bundled* GLFW stay OFF — we use the **system GLFW** (`find_package(glfw3)`, else the system lib + headers) and force the X11/XWayland backend at runtime (`glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11)`), matching how Chrome runs WebGPU here (`--ozone-platform=x11`).
- Compiles `core/src/gpu/DawnWindowContext.{hpp,cpp}` into `dc_gpu`, links the system GLFW, and defines `DC_DAWN_WINDOWED=1`.
- Builds `dc_dawn_window_demo` (the canonical embedding template that replaces the deleted GL `hello_glfw`).

- `DawnWindowContext` — owns a GLFW X11 window + a `wgpu::Surface`/swapchain bound to a `DawnDevice`. Surface is built from `glfwGetX11Display()`/`glfwGetX11Window()` via `wgpu::SurfaceSourceXlibWindow` (mirroring Dawn's own `webgpu_glfw` utils), configured `Fifo`/vsync at the window size. Each `presentFrame()`: renders the Scene through `DawnSceneRenderer` into the device's offscreen target (id 0, **unchanged** headless path), then runs a fullscreen-triangle **blit** pass sampling that target's color view into the swapchain texture (`surface.GetCurrentTexture()`), then `surface.Present()` + `glfwPollEvents()`. The blit (vs. baking pipelines per surface format) keeps the RGBA8Unorm scene pipelines untouched while the swapchain is its native BGRA8Unorm. `readSceneToPng()` (and the static `writeRgbaPng`) dumps the rendered target to a self-contained PNG (no zlib/stb dep) as a pixel proof of the windowed pipeline.

## Architecture

### Target Data Flow (C++ core)

The C++ core owns the scene graph and will own rendering. The intended flow is:
```
Data Source → C++ Processing → Scene Graph → C++ Rendering
```

### Browser Data Flow (`@repo/dc-wasm`, WASM + WebGPU)

The browser path runs the C++ `dc` core compiled to WebAssembly and renders through WebGPU. The retired TypeScript/WebGL2 prototype proved out this flow; `@repo/dc-wasm` now realizes it for real:
```
Worker (ingest)  →  ArrayBuffer batches  →  Main Thread Queue
  →  dc core (WASM, binary parsing)  →  GPU Buffer Sync  →  WebGPU Draw Calls
```

### Binary Ingestion Format (per record)

`[1B op] [4B bufferId (u32 LE)] [4B offsetBytes (u32 LE)] [4B payloadBytes (u32 LE)] [payload]`

Op codes: 1 = append, 2 = updateRange.

### Rendering Pipelines

Pipeline types (owned by the C++ core's `PipelineCatalog`; the retired TS prototype's `pipelines.ts` was the original reference):
- `triSolid@1`, `line2d@1`, `points@1` — vertex-based
- `instancedRect@1`, `instancedCandle@1` — instanced geometry (bars, OHLC)
- `textSDF@1` — SDF text rendering with glyph atlas

### Key Subsystems

- **Transform System (D1.5)** — Affine 2D transforms (column-major mat3) on DrawItems for pan/zoom.
- **Buffer Cache Policy (D6.1)** — Per-buffer byte cap with ring-buffer eviction (`evictFront`, `keepLast`).
- **Recipe System (D7.1/D8.1)** — Declarative chart definitions with deterministic ID allocation. Create/dispose command pairs.
- **Text SDF (D2.3)** — Glyph atlas with shelf packing and SDF generation via distance transform.
- **WebSocket Support (D5.2)** — Worker connects to real server or falls back to fake streams for demos.

### C++ Core Classes

- `Scene` — Owns panes, layers, draw items. Cascading delete, frame atomicity (beginFrame/commitFrame).
- `CommandProcessor` — Parses JSON commands, applies them to the Scene.
- `ResourceRegistry` — Manages GPU resource metadata.
- `PipelineCatalog` — Registers and resolves pipeline types.

- `CpuBufferStore` (`dc/render/`) — Backend-agnostic CPU-side buffer store: per-id CPU bytes, capacity, dirty-range coalescing, `UploadStats`. The Dawn upload path drives it through `GpuDevice` + a `BufferHandleResolver`. (This was the device-neutral half of the old GL `GpuBufferManager`.)

### C++ Dawn Renderer Classes (`dc_gpu`)

- `DawnDevice` — Headless WebGPU/Dawn device: offscreen render target + synchronous pixel readback (`readPixel`, top-down origin), buffers/textures/samplers, pipeline + bind-group creation, blend/clip permutations.
- `DawnSceneRenderer` — Walks the whole Scene and dispatches every DrawItem through the registered Dawn backends (clear, per-pane scissor, per-item frustum-cull + blend + clip + transform, pane borders/separators). The Dawn mirror of the old GL `Renderer::render`. Exposes `render(scene, store, W, H)` and `renderPick(...)`.
- Per-pipeline backends — one `IRendererBackend` per pipeline (`DawnTriSolidBackend`, `DawnLine2dBackend`, `DawnInstancedRectBackend`, `DawnTextSdfBackend`, `DawnTexturedQuadBackend`, …) plus `DawnPickBackend`, all reading CPU bytes from `CpuBufferStore`.

## Conventions

- C++17 required for `dc`; `dc_gpu` and anything including Dawn headers needs C++20 (Dawn's C++ wrapper). Third-party C++ deps: RapidJSON (header-only, JSON parsing). The renderer dep is Dawn, fetched from source when `-DDC_FETCH_DAWN=ON` (see WebGPU/Dawn section). No GLAD / OSMesa / GLFW.
- All TypeScript packages use ESM (`"type": "module"`).
- Workspace packages reference each other via `"workspace:*"` protocol.
- Library packages export directly from `./src/index.ts` (no build step; consumed by Vite).
- D-numbers (D1.1, D2.3, etc.) are informal milestone identifiers used in test names and comments.

## Workflow

- Use `feature/<name>` branches. Features can be stacked. Small changes can go directly on `main`.
- Tests are for regression defense, not coverage targets. Name test files after the feature milestone (e.g., `d1_1_smoke.cpp`).
- **Definition of done includes `LIMITATIONS.md` (ENC-991).** Before opening a PR:
  1. **Did you fix a limitation?** Move its entry to §R with your commit and ticket. Do not
     delete it — a log that silently drops entries is indistinguishable from one nobody
     maintains, which is exactly how the previous attempt died.
  2. **Did you find one?** Add an entry: a `Re-check` command you have actually run, a
     `Verified at <sha>, <date>` stamp, a severity, a workaround, and a ticket (or an explicit
     "None"). If you cannot write the one-line re-check, you have an impression, not a
     limitation. **Its id is `DC-L-<your ENC ticket number>`, not the next free `DC-Lnn`**
     (ENC-1277 — that space is closed at `DC-L18`); `pnpm test` fails if two entries share an
     id, if one vanishes, or if an id is malformed.
  3. **Did you touch a file some entry's `Re-check` names?** Run that re-check and either
     restamp the entry or retire it. This is the whole maintenance burden, and it is scoped to
     files you already have open.
  4. **Did you disprove something everyone believed?** That goes in §C (Corrections), not into
     a commit message nobody will search.

## Design records — the workspace-root SPECs that decided this engine (ENC-1374)

`LIMITATIONS.md` is the **open** log — what is not true of the engine, alive until falsified.
These four are the **closed** ones — what a project *decided*. The rule is stated once, in the
render-correctness SPEC's `### The division of labour with LIMITATIONS.md` section: they point
at each other and never restate each other, so a reader who finds one and not the other will
assume the other does not exist.

They live at the **workspace root**, not in this repo
(`specs/2026-09-14-gsd-proposal-backfill/SPEC.md` D2), which is why each needs a pointer from
here at all (that SPEC's D13, installed per its D14). They sit beside the other service repos in
the workspace checkout — note that a ticket worktree under `trees/<group>/` is *three* levels
below that root, not one.

**Cite these by heading, not by line.** Not because they grow — the render-correctness SPEC says
of itself *"Grows by nothing; this document is finished"* — but because their own source pins
have already drifted against this repo: session-continuity §5 Q3 cites
`core/wasm/dc_engine_host.cpp:624` and `:513-519`, rich-chart §7.3 cites `EngineHost.ts:918`,
and `LIMITATIONS.md` DC-L05 cites `:894`/`:626`. Every one of those now lands on unrelated code.
Re-derive a line before quoting it.

- **`specs/2026-09-14-dynacharting-render-correctness/SPEC.md`** — the primary record for this
  repo. §2's ten locked decisions, of which this repo owns **D1–D6, D7(a) and D8–D10** (only
  D7(b), `stack.sh doctor`'s skipped-target warning, is the workspace's): `lineAA@1` as THE
  default line pipeline driven from `markSpecOf`'s default argument rather than a flag (D2); the
  lineAA quad's perpendicular computed in **pixel** space and single-sourced between shader and
  test as `DC_LINEAA_QUAD_EXPAND` (D3); arc chords sized by angular span at `segmentsPerTurn =
  72` (D4); `--png` dropping text ruled `DOCUMENT, not fix`, deliberately without a test (D5);
  one whole-frame `DawnDevice::readFramebufferRGBA` preferred over a GPU round trip per pixel
  (D6); every `assert()` → `DC_CHECK`, because `NDEBUG` had made five tests vacuous (D7a); a
  wasm rebuild reproducible across checkout *locations* (D8); `getSceneDocument(compact)`
  returning a copied `std::string` rather than a `typed_memory_view` (D9); and
  `LIMITATIONS.md`'s own charter (D10). **Four of these are checkable in a plain default build,
  deliberately** — `dc_enc993_lineaa_default` (D2), `dc_enc981_lineaa_width` (D3),
  `dc_enc995_arc_tessellation` (D4) and `dc_enc984_scene_export` (D9) all link `dc`, not
  `dc_gpu`. D3's own CMake comment says why: *"Deliberately a plain `dc` test (not Dawn-gated) …
  the Dawn lineAA test rendered at 128x128, where this bug is exactly zero"* — for D3 the Dawn
  path is the **blind** one. What a default build cannot show is the *rendered result*: D6's
  measurement is a `dc_json_host` figure, D5 has no test by design, and D2's golden-parity
  companion `dc_enc993_dawn_default_line` links `dc_gpu` and is excluded at *configure* time
  with everything else behind `DC_FETCH_DAWN=OFF` (DC-L01, and the `ctest` warning under **C++
  Core (CMake)** above). §5 **Q1** is the sharper limit: `EncodePass` is not linked into the
  shipped wasm at all, so D2 and D4 hold on the native path and on nothing a browser user sees.
  §5 **Q2** records the twenty recipes reachable only from C++.

- **`specs/2026-06-20-rich-chart-research/SPEC.md`** (with its `GAPS.md`) — the rich-chart
  authoring record. It has **no `D<n>` ids of its own**: its decisions are **`GAPS.md` G1–G8**,
  and the ledger is SPEC **§7.2** (the G-number ↔ ticket mapping, eleven tasks across twelve
  tickets) and **§7.4** (the per-repo split). Six of the eleven landed here: **G1b**, the Y-flip
  in `EngineHost.blitFramebuffer` plus its `EngineHost.blit.test.ts` golden — **G1a** is its
  separate verify harness, `packages/dc-wasm/examples/yaxis_orientation_verify.html`, not the
  fix — and the authoring vocabulary, **G2** `chart/SceneBuilder.ts`, **G4** `chart/scale.ts`,
  **G5a** `chart/ids.ts`, **G5b** `EngineHost.rejections.test.ts`. Of those, `scale.ts` and
  `SceneBuilder.ts` already carry the path; `ids.ts` cites *"GAPS.md G5 / SPEC §5.5"* by
  document name without the path, and the blit site and both test files cite ticket ids only —
  so this line is their path citation. Elsewhere: **G3a** is customer-layer's DOM label overlay;
  **G3b** and **G8** produced **no code** (their output is the two `ENC-70{4,5}-*-spike.md`
  files in that directory — G8 ruled that forum's `pipelinetranslate` already did what was
  needed, so forum shipped nothing under this project); **G6** splits client (customer-layer) /
  server (embassy); **G7** is embassy. Nothing here rests on the Dawn-gated targets — the record
  renders through `@repo/dc-wasm` in Chrome on SwiftShader — but §7.5 **Q4** notes its harness
  needs a local `pnpm --filter @repo/dc-wasm build:wasm`, because `harness/wasm/` is gitignored.

- **`specs/2026-06-21-dynacharting-authoring-corpus/SPEC.md`** — §2 runs **D1–D9**, and four of
  them reach into this repo. **D4** is the sharpest: the corpus reads the framebuffer raw and
  row-flips it itself, which `LIMITATIONS.md` **DC-L05** cannot see — DC-L05's re-check greps
  the literal `core.framebuffer()` under `packages/dc-wasm/src`, while the corpus calls
  `host.framebuffer()` from a file outside this repo (SPEC §5 **Q1**). **Treat DC-L05's "exactly
  two consumers" as a floor, not a census**: that same literal-string grep also misses
  `packages/dc-wasm/examples/engine_host_demo.html` (flips, with its own inline loop) and
  `packages/dc-wasm/examples/yaxis_orientation_verify.html` (deliberately does not flip). When
  DC-L05's deferred deep fix lands, every one of those flips has to come out together, the
  corpus's `runner/gallery.html` included. **D2** is *"the engine build artifact is copied in,
  not committed"* — the corpus's own `runner/dc_engine_host.{js,wasm}` are gitignored, and §5
  **Q3** records the cost: nothing says where the copy comes from, and it is rebuilt here when
  the pipelines move (ENC-713, `9f1d2c9`). **D5** rules that authoring goes through one kit
  rather than raw draw commands; ENC-714 promoted that kit into this repo as
  `packages/authoring-kit/` (`@repo/authoring-kit`). **D9** records that the corpus's
  `docs/LIMITATIONS.md` is the direct ancestor of this repo's file. One caution: §3 calls the
  corpus *"Read only … No corpus ticket has ever edited it"*, and **that sentence is
  contradicted by the SPEC's own §6**, which lists ENC-713, ENC-714 and ENC-715 as `Done`
  DynaCharting tickets. Believe §6.

- **`specs/2026-09-14-session-continuity/SPEC.md`** — this repo holds **one half of one
  decision** and no whole one. §6's ledger gives DynaCharting a single ticket — **ENC-949**
  (`Backlog`, est 5), implementing **D9**, *"the camera producer"*. D9 itself (`scene_state` is
  a server-derived document; camera state must be a **field** of it, never a PATCH replacing it)
  is ruled by forum and customer-layer; what is DynaCharting's is that nothing above the engine
  can *produce* that state — §3's row: pan/zoom lives here, `customer-layer`'s
  `EngineContextValue` is write-only, and transforms arrive server→browser as wire commands.
  Read **§5 Q3 before starting ENC-949**: its premise has moved twice and the ticket says
  neither thing. `getSceneDocument` is already bound through embind and in the shipped `.wasm`
  (ENC-984), but `sceneToDocument` (`core/src/document/SceneExport.cpp`) **never populates
  `viewports`/`DocViewport`**, so the document that now crosses the wasm boundary carries no
  camera; the host bolts on `viewportWidth`/`viewportHeight` from the last `render()`, which is
  surface pixels, not a camera. The `SceneExport.cpp` half is default-build work
  (`dc_enc984_scene_export`); the wasm-host half is not — `dc_engine_host` is gated on `if
  (EMSCRIPTEN)` and needs emsdk.
