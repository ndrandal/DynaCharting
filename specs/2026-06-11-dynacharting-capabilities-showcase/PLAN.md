# DynaCharting Capabilities Showcase — "Frontier" (QUALIFIED v2)

**Status:** Qualified against the codebase by 5 adversarial agents (2026-06-11). Ready to formalize into a Linear project.
**Type:** Public-facing case-study / POC. Demoed broadly. UI is a first-class deliverable.

## 1. Goal & deliverables
Demonstrate the breadth of *data-aware* charts DynaCharting can render via a **faithful mock of the production data path** (mock GMA → embassy → dataplane WS → dc-wasm/WebGPU browser), across **~17 curated views** spanning **native → composed → walled**, presented as a **designed public showcase** with per-view explainers and a **frontier map**.

1. A running, designed interactive browser showcase (the POC).
2. An empirical **frontier report**: per-view native / composed / walled-with-reason — the evidence base for the future "custom-WGSL-pipeline-from-JSON" decision.

## 2. Non-goals
Custom-shader-from-JSON; real GMA/forum/feeds/auth/prod hardening; in-engine data mutation (purity preserved — all computation upstream).

## 3. Qualification results (what changed from v1)

**Confirmed (hold):**
- **A2 ✓** embassy runs forum-less via `EMBASSY_INSTRUCTION_FILE` + unset `EMBASSY_FORUM_URL`. Schema: `{id, sessionId, subscriptions:[{id,streamKey,field}], visualization?}` (`cmd/embassy/main.go` loadInstructionFile).
- **A5 ✓** one engine instance flips across all views: `delete` panes (cascades layers/drawItems) + delete buffers/transforms; IDs reusable after `ResourceRegistry::release`. Needs an `applyManifest()`/`resetScene()` convenience (T5.4a).
- **A9 ✓** the WASM bundle is current and links **all 10 pipelines** incl. texturedQuad + textSDF (`core/CMakeLists.txt` dc_engine_host target).

**Broke / reframed (the important corrections):**
- **A4 (was "critical blocker") → MANAGED.** embassy's GMA value path is **scalar-only** (`func(value float32)`; `asFloat32` drops arrays; GMA `ob.*` returns scalars). Vector/L2 views (depth ladder, market profile, footprint) can't ride a single array subscription. **Reframe: because we own the mock, vector data is emitted as a _fan of scalar keys_** (e.g. `depth.bid.0.size … depth.bid.N.size`), routed into one buffer via embassy's **existing compound-join mechanism** (the same path candle6 uses: many subs → one packed record). So these views are **composed, not walled** — they just need a generalized "multi-sub → buffer-slots" mapping (T0.2/T3.3), not new GMA array support.
- **A3 (partial).** Buffer IDs are **not** specifiable from the instruction file today (legacy path hardcodes `100+index`; recipes own IDs). Fix: add an explicit `bufferId`/slot binding to the instruction file — a small generalization of the recipe mechanism (T0.2 → T3.3).
- **A6 (escape hatch REACHABLE, needs conductor wiring).** `texturedQuad` is test-proven (`d36_1_dawn_texquad.cpp` via a `TextureSource`), but (a) there is **no JSON command to upload texture pixels** and (b) `dc_engine_host.cpp` doesn't wire a `TextureSource` yet. Fix: add a `TextureSource` impl + Embind `setTexturePixels(id,bytes,w,h,fmt)` to the WASM host (~1–2h, proven). This **rescues weather-radar + the 3 "walled" views** into *composed-via-upstream-precompute* (producer rasterizes the colormap → texture; purity preserved). New ticket T3.6.
- **A7 (partial).** `EngineProvider` is clean, but `useLiveUpdates` is coupled to `useTenant`/auth. Fix: make `agentDataUrl` an optional param + a standalone `ShowcaseApp` (no auth/tenant) + `VITE_SHOWCASE_AGENT_URL` (T5.0b, ~3h).
- **Capture (A9/T6.2)** reliable in dev (DISPLAY=:0 + Vulkan + ~/pw) but for 20 automated captures needs a `window.__dcEngine.isSettled()` signal + Playwright polling (not a fixed timeout) + per-view error handling.

**The frontier reframe (better story):** with the scalar-fan + texture-feed mechanisms, **~all 17 views become renderable.** The *true* wall narrows to one sharp thing: **real-time GPU-side computation / novel per-pixel shaders** (live FFT, live KDE density, live marching-squares, glow). We render the *precomputed* versions and document the *live-GPU* gap — which is exactly the custom-pipeline-from-JSON decision. The map gets cleaner, not emptier.

## 4. Architecture (qualified)
```
 Mock GMA (Go, promoted from           embassy (no forum)                     DynaCharting/apps/showcase (browser)
 embassy fakeServer)                    EMBASSY_INSTRUCTION_FILE=view.json     ShowcaseApp (no auth)
  • GMA WS protocol                     EMBASSY_GMA_WS_URL=mock                 • dc-wasm EngineHost (WebGPU)
  • replays datasets ~20s        ws     EMBASSY_FORUM_URL=unset          ws     • applyManifest(catalog[view])
  • scalars + scalar-fans for     ───▶  • binary.go packs 13-byte records ───▶  • enqueueData(records)
    vector data (depth/profile)         • multi-sub → buffer slots             • setTexturePixels() for heatmaps
  • emits precomputed colormap          • (NEW) explicit bufferId binding      • setTransform pan/zoom
    textures for heatmap views          • dataplane WS :8001/data              • designed gallery + explainers + frontier map
```
- **Manifests live in the showcase**; embassy is a generic data pump.
- **Buffer-ID contract:** manifest declares buffer IDs; instruction file binds each subscription (or scalar-fan group) → those IDs.
- **Repo placement (decided):** new pnpm app **`DynaCharting/apps/showcase`** (alongside live-viewer) holds the UI + manifests + datasets + instruction files; the **mock GMA is a standalone Go binary `embassy/cmd/mock-gma`** (promoted from the test `fakeServer`). Minimizes worktree friction; keeps the showcase in DynaCharting's ecosystem.

## 5. Phases & tickets (all ≤5h)

### Phase 0 — Design & contract
- **T0.1** Buffer-ID contract spec (manifest ↔ instruction-file; scalar vs scalar-fan vs compound vs texture).
- **T0.2** embassy spike (RISK, do FIRST): generalize "subscription/scalar-fan → explicit buffer IDs/slots" without bespoke per-view recipes; scope the code (feeds A3/A4).
- **T0.3** Repo scaffold: `apps/showcase` skeleton + `cmd/mock-gma` skeleton + run scripts.

### Phase 0.5 — Vertical slice (NEW, de-risks A1/A3/A4/A5/A7)
- **T0.5** One dataset → mock GMA → embassy (forum-less) → one OHLC view → browser renders end-to-end. The working baseline before fan-out.

### Phase 1 — Datasets
- **T1.1** Dataset format + **market** generator (multi-symbol tick→OHLCV + L2-as-scalar-fan + TA).
- **T1.2** **Synthetic non-equities** datasets (2D scientific field, ECG/audio trace, sports shot) + precomputed colormap textures for heatmap views.
- **T1.3** Dataset inspector/validation.

### Phase 2 — Mock GMA
- **T2.1** Promote `fakeServer` → standalone `cmd/mock-gma` (subscribe/cancel/update, reconnect).
- **T2.2** Dataset replay engine (~20s, realistic cadence, scalar + scalar-fan emission).
- **T2.3** Fidelity tests (embassy talks unchanged).

### Phase 3 — embassy harness
- **T3.1** Per-view instruction files + subscription→bufferID/slot mapping.
- **T3.2** Forum-less run script (env wiring; per-view sessions).
- **T3.3** Implement the generic explicit-bufferId / multi-sub→buffer-slots path (from T0.2).
- **T3.4** Dataplane WS verification.
- **T3.5** Manifest + instruction-file validator (schema lint across all views).
- **T3.6** (NEW) WASM `TextureSource` + Embind `setTexturePixels()` — unlock texturedQuad in the browser (the heatmap/spectrogram escape hatch).

### Phase 4 — View catalog & manifests (split per tier; each ≤5h)
- **T4.1** Native A: OHLC bars; candle+volume+EMA/SMA; line/area/baseline.
- **T4.2** Native B: multi-pane price+RSI+MACD; scatter.
- **T4.3** Composed (scalar-fan): order-book depth ladder; volume/market profile (TPO); footprint cluster.
- **T4.4** Composed (tessellated): renko/P&F; sector treemap; ridgeline/horizon; streamgraph.
- **T4.5** Composed (tessellated): Sankey ribbons; radial seasonality; correlation heatmap.
- **T4.6** Cross-domain: weather-radar heatmap (texture); ECG trace; audio waveform; sports shot chart.
- **T4.7** Walled-probe tier: GPU density/liquidity heatmap; contour/isolines; interpolated spectrogram — render precomputed-texture versions; document the live-GPU-compute wall.

### Phase 5 — Browser showcase app (design-first, public)
- **T5.0** (BLOCKING) Design direction + narrative arc: visual language (brand/type/color), case-study story (engine → frontier map → why the limit matters), wireframes, landing/intro, CTA.
- **T5.0b** Public-route decoupling: `useLiveUpdates(agentDataUrl?)` + standalone `ShowcaseApp` (no auth) + `VITE_SHOWCASE_AGENT_URL`.
- **T5.1** Showcase shell & component system (cards, badges, layout).
- **T5.2** Gallery / tier-organized view-switcher.
- **T5.3** Per-view explainer panel (title, reference tool, "what's going on", tier badge, buffer/pipeline facts).
- **T5.4** Engine integration: `applyManifest()`/`resetScene()` helper, manifest switch, WS resubscribe, pan/zoom.
- **T5.5** Frontier-map page (the headline artifact).
- **T5.6** Polish: replay control (play/pause/restart synced to the 20s timeline), loading/empty/error states, responsive.
- **T5.7** Accessibility (ARIA, keyboard view-nav) + WebGPU-unsupported friendly fallback.

### Phase 6 — Wire-up, capture & report
- **T6.1** One-command bring-up (mock-gma + embassy + showcase).
- **T6.2** Still-frame capture: instrument `isSettled()`; Playwright polls + per-view error handling → contact sheet.
- **T6.3** Frontier report (rendered / composed-how / walled-with-reason; escape-hatch findings; custom-pipeline recommendation).
- **T6.4** Runbook/README + architecture diagram.
- **T6.5** "How to add a view" contributor walkthrough.
- **T6.6** Perf budget: views live, FPS target or documented bottleneck; browser-compat matrix.

## 6. Risks (updated)
- **R1 (was critical) → managed:** scalar-only value path. Mitigated by scalar-fan + compound routing + explicit-bufferId binding (T0.2/T3.3). Validate in T0.5.
- **R2 (med):** texture-feed needs the Embind `TextureSource` wiring (T3.6) — proven but not free; if descoped, heatmap/spectrogram/weather revert to walled.
- **R3 (med):** tessellation burden for polar/Sankey/contour/treemap — computed upstream (producer/manifest-time), not in-engine.
- **R4 (low):** scene-reset correctness across 20 views — helper + T0.5 proof.
- **R5 (low):** public-route decoupling (T5.0b); capture-at-scale needs `isSettled()` instrumentation.

## 7. Scope reality
~33 tickets ≤5h (≈150–165h of work). Heavily parallelizable across worktrees/agents: datasets ∥ mock-GMA ∥ design-direction ∥ (after T0.5) the 7 view tickets. Critical path: T0.1→T0.2→T0.5→{T3.3,T3.6}→T4.*→T5.4→T6.*. Front-load **T0.2 (risk spike)** and **T0.5 (vertical slice)**.

## 8. Validation
Each phase self-validatable: build/test/run + Playwright capture (~/pw, Chrome x11/Vulkan, DISPLAY=:0). Native dc core/Dawn suites unaffected.
