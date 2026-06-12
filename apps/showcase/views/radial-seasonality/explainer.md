---
title: Radial Seasonality Clock — the clock sweeps
referenceTool: polar / radial seasonality clock
tier: composed
---

A polar seasonality clock that **sweeps** — average per-tick return by cyclic phase bin, drawn around a 24-wedge dial as **filled radial wedges** (a polar rose). A clock hand advances 0→360° over the 20 s timeline, and as it passes each phase bin that bin's wedge is revealed (its radius eases from the baseline ring out to the bin's seasonal value). The engine has **no native polar transform** (it is affine-2D only), so the polar chart is achieved by *projecting* each `(angle, radius)` to cartesian at **build time**: bin `k` → angle θ = 2π·k/24, radius = the bin's scaled average return, giving `x = r·sinθ`, `y = r·cosθ` (0 at 12 o'clock, clockwise). The projected wedges are filled triangles drawn with the ordinary `triGradient@1` pipeline (`pos2_color4`).

| | |
|---|---|
| **DATA** | per-tick returns of all 4 symbols, folded onto 24 synthetic session-clock bins |
| **PIPELINE** | `triGradient@1` (filled triangle list, per-vertex color) |
| **WRITE MODE** | LIVE geometry-frame replay — one `UPDATE_RANGE` (op 2, offset 0) per frame overwrites the whole wedge buffer as the hand sweeps |
| **COMPOSED VIA** | build-time polar→cartesian projection (`x=r·sinθ, y=r·cosθ`) + per-frame re-tessellation in `records.gen.mjs` |
| **BUFFERS** | `601` pos2_color4 wedge fans (24 wedges × 5 fan-tris) + sweep hand |
| **SOURCE** | precomputed from `data/market/*` by `.build-composed-static.mjs`; swept by `records.gen.mjs` |

**What's going on (the technique).** Polar is the canonical "the renderer can't do this transform" case — and it doesn't need to. Each cyclic bin's value is projected to a cartesian fan of triangles on the host before any GPU work, so the colored-triangle pipeline draws the radial rose exactly as it would draw any 2D filled geometry. The clock SWEEPS via a **geometry-frame replay**: `records.gen.mjs` re-tessellates the rose at 72 timesteps (a full revolution over the timeline) and `records.json` carries one full-buffer `UPDATE_RANGE` per frame. ENC-569's triGradient backend re-reads + redraws the overwritten buffer each frame, so the wedges fill in as the hand passes. The vertex count is constant across frames (un-revealed wedges sit at the baseline radius), so the buffer is pre-sized once and every frame is a stable full-buffer overwrite at offset 0 — no `growth` needed.

> **Synthesized session clock.** The 20 s tapes carry no real calendar seasonality, so the cyclic axis is synthesized: every `lastPrice` tick's timestamp is folded (mod the tape duration) onto one of 24 phase bins and the per-tick % return is averaged within each bin. The radius is that average affinely scaled into a visible clip-space band about the baseline ring. This demonstrates the polar *projection* + live sweep mechanism — not a seasonality finding.
