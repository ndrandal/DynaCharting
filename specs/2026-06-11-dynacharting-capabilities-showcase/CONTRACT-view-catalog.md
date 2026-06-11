# Contract — view catalog & capture/replay (wave 3 integration)

The architecture that lets the gallery hold ~17 views without N live backends.

## Model: capture-once, replay-in-browser
- **Capture (build-time, per view):** run the *real* faithful pipeline once — mock-GMA → forum-less embassy (the view's `instruction.json`) → record ~20s of dataplane binary frames (with relative timestamps) → `records.json`. The pipeline is exercised for real; the output is frozen.
- **Replay (browser, runtime):** the gallery loads a view's `manifest` + `records.json`, applies the manifest, and replays the recorded frames via `EngineHost.enqueueData` on a timeline (play/pause/restart). No live backend needed → instant switching + static-deployable + faithful.

## Per-view directory: `apps/showcase/views/<view-id>/`
- **`view.json`** — `{ id, title, tier: "native"|"composed"|"refType", referenceTool, blurb, datasetId, transform?: {sx,sy,tx,ty}, xAnchor?: boolean }`. (`tier` semantics + colors per DESIGN-showcase-ui.md: native green / composed blue / walled amber.)
- **`manifest.ts`** (or `.json`) — a `SceneManifest` (existing type in `src/scene/commands.ts`): the ordered SceneDocument commands. **No `uploads`** — data arrives via replay.
- **`instruction.json`** — the embassy `showcase-explicit-v1` instruction (subscriptions + buffer bindings) the capture harness feeds embassy. Buffer IDs MUST match the manifest (per CONTRACT-buffer-id.md).
- **`records.json`** — captured output: `{ meta:{viewId,durationMs,frameCount,cadenceMs}, frames:[{ t:<msOffset>, b64:<base64 of the binary frame> }] }`. Binary frames are embassy's dataplane records (13-byte header + payload); replay decodes b64 → ArrayBuffer → `enqueueData`.
- **`explainer.md`** — front-matter + body per the design doc's explainer pattern: `title`, `referenceTool`, a one-sentence **DATA + TECHNIQUE** "what's going on", `tier`, and the buffer/pipeline facts (e.g. "candle6 buffer, instancedCandle@1, OHLC compound join").

## Registry & app integration
- **`apps/showcase/src/views/registry.ts`** — imports every view (its `view.json`, `manifest`, `records.json`, `explainer`) into a typed array `VIEWS: ShowcaseView[]`. The gallery iterates this; the engine loads the selected one.
- **Switching** (extends the proven slice): `resetScene(host, prev)` → `applyManifest(host, view.manifest)` → start `useReplay(host, view.records)`. Transform framing: bake `view.json.transform` (and `xAnchor` for record-index X), since the showcase-explicit path has no RangeTracker.
- **Replay engine** — `useReplay(host, records, {playing, onProgress})`: decodes frames, schedules `enqueueData` at each frame's `t`, supports restart/pause. This is the T5.6 replay control's data source.

## Capture harness: `apps/showcase/tools/capture.mjs <view-id>`
Builds+runs `cmd/mock-gma` + `cmd/embassy` (both from the embassy main checkout) forum-less with the view's `instruction.json`, connects a WS client to the dataplane, records frames+timestamps for `meta.durationMs`, writes `records.json`, tears the processes down by PID. (Reuses the slice's run recipe — see EXECUTION LOG in memory / the slice PR. Note: no `ws` npm pkg at ~/pw; vendor a small WS client or `npm i ws` in the tool dir.)

## Why this shape
- Faithful (capture runs the real pipeline) + robust (gallery has no live deps) + deployable + instant-switch + gives replay-controls and still-frame capture for free.
- View agents own `view.json`/`manifest`/`instruction`/`explainer` + run capture → `records.json`. UI agents own the gallery/explainer/frontier-map/replay-controls against `registry.ts`. They meet at this contract.
