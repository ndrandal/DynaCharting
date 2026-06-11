---
title: Radial Seasonality Clock — polar projection
referenceTool: polar / radial seasonality clock
tier: composed
---

A polar seasonality clock — average per-tick return by cyclic phase bin, drawn around a 24-spoke circle with a zero-return baseline ring. The engine has **no native polar transform** (it is affine-2D only), so the polar chart is achieved by *projecting* each `(angle, radius)` to cartesian at **manifest-build time**: bin `k` → angle θ = 2π·k/24, radius = the bin's scaled average return, giving `x = r·cosθ`, `y = r·sinθ`. The projected points are then drawn with the ordinary `line2d@1` pipeline (`pos2_clip`).

| | |
|---|---|
| **DATA** | per-tick returns of all 4 symbols, folded onto 24 synthetic session-clock bins |
| **PIPELINE** | `line2d@1` (DrawMode Lines — independent segments) |
| **WRITE MODE** | static `uploads` — geometry baked once at manifest time |
| **COMPOSED VIA** | build-time polar→cartesian projection (`x=r·cosθ, y=r·sinθ`) in `manifest.ts` |
| **BUFFERS** | `601` pos2_clip line segments (ring + spokes + series loop) |
| **SOURCE** | precomputed from `data/market/*` by `.build-composed-static.mjs` |

**What's going on (the technique).** Polar is the canonical "the renderer can't do this transform" case — and it doesn't need to. Each cyclic bin's value is projected to a cartesian point on the host before any GPU work, so the line pipeline draws the radial series exactly as it would draw any 2D polyline. The buffer holds three things in one `pos2_clip` segment list: the baseline ring (a zero-return reference circle), one spoke per bin (center → projected point), and the closed series loop through all 24 projected points. Because `line2d@1` uses `DrawMode::Lines` (vertex *pairs*, not a strip), each edge is emitted as two explicit vertices.

> **Synthesized session clock.** The 20 s tapes carry no real calendar seasonality, so the cyclic axis is synthesized: every `lastPrice` tick's timestamp is folded (mod the tape duration) onto one of 24 phase bins and the per-tick % return is averaged within each bin. The radius is that average affinely scaled into a visible clip-space band about the baseline ring. This demonstrates the polar *projection* mechanism — not a seasonality finding.
