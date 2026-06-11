# DynaCharting · Frontier — the capabilities showcase

A designed, public-facing gallery that demonstrates the breadth of **data-aware
charts** DynaCharting renders from **JSON manifests** over a **faithful
market-data path** — 22 views across three tiers (native / composed / walled),
each a live WebGPU render in the browser.

This is the showcase app (`@repo/showcase`). For the case-study writeup and the
frontier findings, see
[`specs/2026-06-11-dynacharting-capabilities-showcase/REPORT.md`](../../specs/2026-06-11-dynacharting-capabilities-showcase/REPORT.md).
The captured visual proof is the [contact sheet](./stills/contact-sheet.html)
([PNG](./stills/contact-sheet.png)).

---

## Run it

From the **repo root** (this is a pnpm workspace):

```bash
pnpm install                              # install all workspace deps (once)
pnpm --filter @repo/showcase dev          # vite dev server (HMR) -> http://localhost:5174
# — or —
pnpm --filter @repo/showcase build        # tsc -b && vite build -> apps/showcase/dist
pnpm --filter @repo/showcase preview      # serve the production build
```

Open the printed URL in a **WebGPU-capable browser** (see *Browser support*
below). The app is a static deploy — `vite build` produces a self-contained
`dist/` (HTML + JS + the `dc_engine_host.wasm` engine + the per-view records).

### Keyboard

`space` play/pause · `r` restart · `<-`/`->` prev/next view (in single-view mode).

### Routes (hash router, static-deployable)

| Route | Act | What |
|---|---|---|
| `#/` | Hero | thesis + live flagship view + data-path spine |
| `#/gallery` | Gallery | tier-grouped grid of all views |
| `#/view/:id` | Single view | the live canvas + per-view explainer + replay transport |
| `#/frontier` | Frontier map | the payoff — what's native / composed / walled, and why |
| `#/report` | Report | the plain close + "how to add a view" |

## Architecture

The showcase never talks to a live backend at runtime. Instead it uses a
**capture-once / replay-in-browser** model so the gallery can hold 22 views
without 22 live services, stay static-deployable, and still be *faithful* — the
records it replays were produced by the real production-shaped pipeline.

### The faithful data path (capture time)

```
cmd/mock-gma  --ws-->  forum-less cmd/embassy  --dataplane /data ws-->  capture client
   (the feed)            (the view's instruction.json,                    (records ~20s of
                          EMBASSY_FORUM_URL unset)                         binary frames)
```

- **mock-GMA** — a synthetic market feed (built from the embassy MAIN checkout; never modified).
- **embassy** — run **forum-less** via `EMBASSY_INSTRUCTION_FILE` + an unset `EMBASSY_FORUM_URL`: it applies the view's `instruction.json` (subscriptions + buffer bindings), subscribes to the feed, and emits binary dataplane records on its `/data` websocket.
- The capture client records every binary frame with a relative timestamp into the view's `records.json`. Text frames (embassy's own scene-init) are ignored — the showcase applies its *own* manifest.

### The render path (browser, runtime)

```
records.json (replayed on a timeline)
   --> dc-wasm EngineHost.enqueueData (the C++ dc core, compiled to WASM)
   --> GPU buffer sync --> WebGPU draw calls (the dc_gpu renderer, via Dawn in-browser)
```

One **EngineHost** drives one `<canvas>`, which is **portaled** into whichever
route slot is active (`EngineCanvas`) so the engine survives route changes — no
churn (`DESIGN-showcase-ui.md` section 6, "one engine"). On view-select,
`useViewSwitch` calls `resetScene` -> `applyManifest` -> bakes the view's
transform, then `useReplay` loop-replays the captured records (ambient motion,
no drift). When WebGPU is absent, a designed still-image fallback renders.

### Tiers

- **native** — the engine renders directly from the manifest (instanced/vertex/text pipelines over streamed records).
- **composed** — JSON manifest **+ an upstream transform**: the *scalar-fan / fixed-mode* write (current-state vectors), *build-time tessellation* (`triGradient@1` colored triangles), *build-time projection* (polar -> cartesian), or the *texture escape hatch* (`texturedQuad@1` + `setTexturePixels`). The engine stays pure — all computation is upstream.
- **walled** — rendered from a **precomputed** field (a colormap texture); the *live-GPU per-pixel* version (live FFT / KDE / marching-squares) is the frontier. See the report.

## The capture harnesses (`tools/`)

Two Node tools, neither needs a graphics API beyond the browser one uses:

### `tools/capture.mjs` — capture-once (per view)

Freezes one view's dataplane output to its `records.json`:

```bash
node apps/showcase/tools/capture.mjs <view-id> [--duration 20000] [--cadence 75] \
  [--gma-port N] [--data-port N]
```

It builds mock-gma + embassy from the embassy MAIN checkout (cached; never
modified — `EMBASSY_REPO`, default `/home/ndrandal/workspace/Github/embassy`),
spawns mock-gma then forum-less embassy with the view's `instruction.json`,
waits for `orchestrator.apply.ok` + `embassy.gma.subscribe.sent`, connects a
dataplane WS client, records binary frames for `--duration` ms, writes
`records.json`, and **tears both processes down by PID** (SIGTERM -> SIGKILL,
never a broad `pkill`).

### `tools/snap-stills.mjs` — still capture + contact sheet (the visual proof)

Drives the **built** showcase (served by `vite preview`) through a real WebGPU
Chrome and screenshots every view's live canvas:

```bash
# 1. build + serve
pnpm --filter @repo/showcase build
pnpm --filter @repo/showcase preview          # note the port it prints (e.g. 5178)

# 2. capture all views (Playwright lives in the ~/pw harness; PLAYWRIGHT_DIR overrides)
DISPLAY=:0 SHOWCASE_URL=http://localhost:5178/ \
  node apps/showcase/tools/snap-stills.mjs --wait 8500

# 3. stop preview by PID (do NOT broad-pkill)
kill <preview-pid>
```

For each view it navigates to `#/view/<id>`, waits `--wait` ms for the
loop-replay to settle, samples the canvas (coverage + chroma) to classify the
render full / partial / none, and writes `stills/<view-id>.png`. It then
assembles `stills/contact-sheet.html` (a tiered, tiled grid with title + tier +
verdict badges) and `stills/render-tally.json`. A small `VERDICT_OVERRIDE` map
pins a couple of human-verified verdicts where thin 1px strokes under-report.

Chrome flags (the proven local WebGPU config):
`--ozone-platform=x11 --enable-unsafe-webgpu --ignore-gpu-blocklist --enable-features=Vulkan,WebGPU --use-vulkan --no-sandbox`,
`headless:false`, `DISPLAY=:0`.

## How to add a view

**Adding a view = dropping a `views/<id>/` directory.** The registry
(`src/views/registry.ts`) auto-discovers every view via Vite `import.meta.glob`
— **no edit to any source file.** A directory is included only once it has all
the required files (a half-added view is skipped, not crashed), and views are
sorted native -> composed -> walled, then by title.

Create `apps/showcase/views/<id>/` with these files:

| File | What |
|---|---|
| `view.json` | metadata: `{ id, title, tier, referenceTool, blurb, datasetId, transform?, xAnchor? }`. `tier` is `"native" / "composed" / "walled"`; `transform` is the baked data->clip `{sx,sy,tx,ty}` (the showcase-explicit path has no RangeTracker, so framing is baked per view). |
| `manifest.ts` | exports `manifest: SceneManifest` (the ordered SceneDocument commands) + optional `growth: GrowthSync`. Buffer IDs **must** match the instruction (see `CONTRACT-buffer-id.md`). Static views may carry `uploads`/`textures` instead of replaying. |
| `instruction.json` | the embassy `showcase-explicit-v1` instruction (subscriptions + buffer bindings) the capture harness feeds embassy. Only needed for *captured* (streamed) views. |
| `records.json` | the captured dataplane frames the replay engine plays: `{ meta:{viewId,durationMs,frameCount,cadenceMs}, frames:[{t,b64}] }`. Produce it with `tools/capture.mjs <id>`. Build-time/static views can ship a tiny placeholder. |
| `explainer.md` | front-matter (`title`, `referenceTool`, `tier`) + a one-sentence DATA + TECHNIQUE "what's going on" + the buffer/pipeline fact block. |

Then re-capture the still (`tools/snap-stills.mjs`) to refresh the contact
sheet, and it appears in the gallery, the single-view filmstrip, and the
frontier map automatically.

> **Build-time / static views** (treemap, ridgeline, renko, sankey, ECG, …)
> skip capture entirely: `manifest.ts` imports a dataset, tessellates/projects
> the geometry in deterministic TypeScript, and emits it as static `uploads`
> (and `textures` for the escape-hatch views). These have no live replay.

## Browser / WebGPU support

The showcase renders via **WebGPU** (the C++ `dc_gpu` renderer compiled to WASM,
talking to the browser's WebGPU). It therefore needs a **WebGPU-capable
browser**:

- **Chrome / Chromium** (recommended) — WebGPU ships enabled on supported
  platforms. On a Linux box with a software/Vulkan adapter, launch with
  `--enable-unsafe-webgpu --ignore-gpu-blocklist --enable-features=Vulkan,WebGPU
  --use-vulkan` (this is exactly how the still-capture harness runs).
- **Edge** — same engine as Chrome; works.
- **Firefox / Safari** — WebGPU support is improving but not guaranteed; treat as
  best-effort.

When WebGPU is unavailable the app detects it (`isWebGPUSupported()`), shows a
"No WebGPU" status, and renders the designed still-image fallback instead of a
live canvas — the gallery, frontier map, and report stay navigable.
