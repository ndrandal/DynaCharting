# DynaCharting Capabilities Showcase — "Frontier" (DRAFT for qualification)

**Status:** DRAFT v1 — to be qualified by adversarial agents against existing code before finalizing.
**Type:** Public-facing case-study / POC. Demoed broadly. UI must look designed and be self-explanatory.
**Date:** 2026-06-11

## 1. Goal

Demonstrate the breadth of *data-aware* charts DynaCharting can render, driven by a **faithful mock of the real production data path** (mock GMA → embassy → dataplane WebSocket → dc-wasm/WebGPU browser), across **~15–20 curated views** spanning a deliberate difficulty gradient (**native → composed → walled**), presented as a **polished public showcase** with per-view explainers and a **frontier map** of what the JSON manifest can and cannot express.

Two deliverables:
1. A running, good-looking interactive browser showcase (the POC).
2. An empirical **frontier report**: for each view, native / composed / walled-with-reason — the evidence base for the future "custom-WGSL-pipeline-from-JSON" decision.

## 2. Non-goals

- Building custom-shader-from-JSON. Walled views are **documented, not rendered**.
- Real GMA, real forum, real market feeds, auth, production hardening.
- Mutating data in the engine (data purity preserved; all computation upstream in the mock/datasets).

## 3. Architecture (the confirmed inversion)

```
 Mock GMA (GMA WS protocol)  →  embassy (no forum)  →  dataplane WS :8001/data  →  Browser showcase (dc-wasm/WebGPU)
 replays datasets as            EMBASSY_INSTRUCTION_FILE   raw 13-byte records       applyControl(manifest catalog)
 {type:update,requestId,value}  EMBASSY_GMA_WS_URL=mock                               enqueueData(records)
 over ~20s                      EMBASSY_FORUM_URL=unset                               setTransform pan/zoom
                                binary.go packs records                               designed gallery + explainers
```

- **Manifests live in the showcase** (browser-side catalog of SceneDocuments we author). **embassy is a generic data pump.**
- **Buffer-ID contract:** each view's manifest declares buffers with specific IDs; the matching embassy instruction file maps GMA subscriptions → those buffer IDs by simple `APPEND`; packed formats (candle6) reuse embassy's existing compound path.

## 4. Load-bearing ASSUMPTIONS to qualify (agents: try to break these)

- **A1.** embassy's `fakeGMA`/`fakeServer` test double can be promoted to a standalone long-running mock GMA with no embassy code change; embassy talks to it unchanged via `EMBASSY_GMA_WS_URL`.
- **A2.** `EMBASSY_INSTRUCTION_FILE` + `EMBASSY_FORUM_URL` unset lets embassy run forum-less with a static instruction set.
- **A3.** embassy can stream a **simple subscription → specific buffer ID via APPEND** without a bespoke Go recipe per view (generic per-buffer path). If not, a small embassy addition is in scope (T3.3).
- **A4.** embassy's GMA value path carries the data each view needs. **RISK:** the gma client value callback is `func(value float32)` — *scalar*. Order-book depth / arrays may not flow. Qualify whether array/L2 values can reach a buffer.
- **A5.** dc-wasm `EngineHost` supports a clean **scene reset** between views (delete cascade) so one engine instance can flip across ~20 manifests.
- **A6.** The `texturedQuad` pipeline can be fed a **data texture via the ingest/data path** (the escape hatch for heatmaps/spectrograms). Qualify whether a texture's pixels can be supplied from a buffer/manifest or only from a file/CPU-side C++.
- **A7.** A public, no-auth `/showcase` route can reuse `EngineProvider`/`useLiveUpdates` without the tenant/auth plumbing.
- **A8.** GMA emits (or the mock can emit) every key the views need: ohlc, vwap, sma_N, ema_N, rsi, macd*, bb_*, volume, ob.* — and synthetic non-equities values.
- **A9.** Still-frame capture works via Playwright screenshot of the settled frame (we have ~/pw + Chrome x11/Vulkan flags).
- **A10.** Each composed view's geometry is reachable from the 10 pipelines + 7 vertex formats + transforms + the simple-append buffer model. (Per-view classification to verify in Phase 4 qualification.)

## 5. Phases & tickets (≤5h each)

### Phase 0 — Design & contract
- **T0.1** Buffer-ID contract spec: manifest ↔ instruction-file agreement, simple-append vs compound-packed model, per-view file layout, naming.
- **T0.2** embassy spike (RISK): confirm/define the generic "subscription → bufferID simple APPEND" path; scope any embassy code needed (feeds A3, A4).
- **T0.3** Repo placement + scaffold: decide where mock-GMA / datasets / showcase app / manifests live (new top-level `showcase/`? customer-layer route? standalone vite app?). Scaffold skeleton + run/build scripts.

### Phase 1 — Datasets
- **T1.1** Dataset format + **market** generator: multi-symbol tick→OHLCV + order-book L2 + TA (vwap/sma/ema/rsi/macd/bb/volume), sized for ~20s replay.
- **T1.2** **Synthetic non-equities** datasets: a 2D scientific field over time, an ECG/audio waveform trace, sports shot/scatter data — same format.
- **T1.3** Dataset inspector/validation (ranges, cadence, completeness).

### Phase 2 — Mock GMA
- **T2.1** Promote `fakeGMA` → standalone mock GMA server (subscribe/cancel/update, GMA WS protocol).
- **T2.2** Dataset replay engine: ~20s of value updates at realistic cadence; emit the keys each view needs (precomputed in datasets to preserve purity).
- **T2.3** Mock fidelity tests: embassy talks to it unchanged; value frames match GMA's shape.

### Phase 3 — embassy showcase harness
- **T3.1** Instruction files per view + subscription→bufferID mapping (embassy side of the contract).
- **T3.2** Forum-less embassy run script (env wiring; per-view or multi-session).
- **T3.3** (conditional on T0.2) implement generic per-buffer subscription path in embassy.
- **T3.4** Dataplane WS verification (records land, IDs correct, cadence).

### Phase 4 — View catalog & manifests (grouped by tier; ~2–3 native/ticket, 1–2 composed/ticket, 1 walled/ticket)
- **Native (~5):** candle+volume+EMA/SMA; OHLC bars; line/area/baseline; multi-pane price+RSI+MACD; scatter.
- **Composed (~8–10):** order-book depth ladder; volume/market profile (TPO); footprint cluster; renko/P&F; sector treemap; ridgeline/horizon; streamgraph; Sankey ribbons; radial seasonality; correlation heatmap (instancedRect grid); + cross-industry: weather-radar heatmap, ECG trace, audio waveform, sports shot chart.
- **Walled (~3–4, documented):** GPU density/liquidity heatmap (glow); contour/isolines; interpolated spectrogram — attempt `texturedQuad`-data-texture escape hatch; document where it works/fails.
- Each view: manifest JSON + buffer-ID contract + instruction file + explainer copy (what's going on + reference tool) + tier classification.

### Phase 5 — Browser showcase app (public-facing, design-first)
- **T5.1** Showcase shell & design system: layout, branding, typography, color, "case study" framing; public route (no auth).
- **T5.2** Gallery / view-switcher: organized by tier, clear labels, current-view highlight, thumbnails or nav.
- **T5.3** Per-view explainer panel: title, reference tool, "what's going on" (data + technique), tier badge, buffer/pipeline facts.
- **T5.4** Engine integration: EngineProvider reuse, scene reset on switch, applyControl(manifest), WS subscription switch, pan/zoom.
- **T5.5** Frontier-map page: visual summary of native/composed/walled across all views — the headline case-study artifact.
- **T5.6** Polish: responsive, loading states, the 20s "play then settle" affordance, replay control, optional HUD.

### Phase 6 — Wire-up, capture & report
- **T6.1** End-to-end run orchestration: one command brings up mock GMA + embassy harness + showcase; all views runnable.
- **T6.2** Still-frame capture: per-view PNG via Playwright (settled frame after 20s) → contact sheet.
- **T6.3** Frontier report: the empirical map (rendered/composed/walled-with-reason), escape-hatch findings, the custom-pipeline recommendation. The case-study writeup.
- **T6.4** Runbook/README: run the showcase, add a view, architecture diagram.

## 6. Risks
- **R1 (high):** embassy generic per-buffer subscription path may need new code (A3/T0.2/T3.3).
- **R2 (high):** scalar-only GMA value callback may block array/L2 views (A4) — order book, depth.
- **R3 (med):** texturedQuad data-texture feed may not be reachable from the data path (A6) — shrinks escape hatch, grows walled tier.
- **R4 (med):** multi-view scene reset correctness in one engine instance (A5).
- **R5 (low):** WASM build currency; public no-auth route plumbing (A7).

## 7. Validation
Each phase self-validatable: build/test/run + Playwright visual capture. Browser pixel checks via ~/pw + Chrome x11/Vulkan flags. Native dc core/Dawn suites unaffected.
