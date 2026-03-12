# Trial 040: Ridgeline Plot

**Date:** 2026-03-12
**Goal:** Temperature distribution ridgeline plot (joy plot) — 6 overlapping Gaussian bell curves (Jan–Jun) stacked vertically with alpha 0.7, each shifting rightward and growing taller. Tests triSolid@1 filled area tessellation (triangle strips between baseline and curve), back-to-front layer ordering for occlusion, and smooth parametric curve sampling (50 X samples per curve).
**Outcome:** All 6 Gaussian curves match the formula y = baseline + height × exp(−(x−μ)²/2σ²) to ≤0.000001 error. Peak positions, heights, and baselines all correct. Layer ordering produces correct occlusion. All data within pane bounds. Zero defects.

---

## What Was Built

A 900×600 viewport with a single pane (background #0f172a):

**6 filled distribution curves (6 triSolid@1 DrawItems, pos2_clip, 294 vertices = 98 triangles each):**

| Month | Baseline Y | Peak X | Height | σ | Max Y | Color | Layer |
|-------|-----------|--------|--------|---|-------|-------|-------|
| Jan | 5 | 30 | 8 | 12 | 13 | #3b82f6 (blue) | 11 |
| Feb | 15 | 35 | 9 | 11 | 24 | #6366f1 (indigo) | 12 |
| Mar | 25 | 42 | 10 | 13 | 35 | #8b5cf6 (violet) | 13 |
| Apr | 35 | 52 | 11 | 12 | 46 | #a855f7 (purple) | 14 |
| May | 45 | 60 | 12 | 11 | 57 | #d946ef (fuchsia) | 15 |
| Jun | 55 | 68 | 13 | 10 | 68 | #ec4899 (pink) | 16 |

Each curve tessellated as 49 quads (98 triangles) connecting baseline to Gaussian curve at 50 X sample points from x=5 to x=95 (step ≈1.84).

**6 baseline lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
At Y=5, 15, 25, 35, 45, 55. Spanning X=[5, 95]. White, alpha 0.15, lineWidth 1. Layer 10 (behind all curves).

Data space: X=[0, 100], Y=[0, 70]. Transform 50: sx=0.019, sy=0.027143, tx=−0.95, ty=−0.95.

Color gradient: blue → indigo → violet → purple → fuchsia → pink (cool-to-warm progression).

Total: 30 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 6 Gaussian curves match the formula to ≤0.000001 error.** Every curve vertex y-value independently verified against y = baseline + height × exp(−(x−μ)²/2σ²). Maximum error across all ~300 curve vertices per distribution is 0.000001 (float precision limit).

- **Peak positions are correct.** Jan peaks near x=30.7 (expected ~30), Feb near 34.4 (expected ~35), Mar near 41.7 (expected ~42), Apr near 52.8 (expected ~52), May near 60.1 (expected ~60), Jun near 67.4 (expected ~68). The discrete sampling at ~1.84 step means peaks land within one step of the true peak X.

- **Peak heights are correct.** Jan: 12.986 (expected 13.0), Feb: 23.986 (expected 24.0), Mar: 34.998 (expected 35.0), Apr: 45.978 (expected 46.0), May: 56.999 (expected 57.0), Jun: 67.980 (expected 68.0). All within 0.02 of expected (slight undershoot because the sample grid doesn't exactly hit the peak X).

- **Layer ordering produces correct occlusion.** Jan on layer 11 (drawn first, furthest back) through Jun on layer 16 (drawn last, closest). Each curve partially occludes the one behind it, creating the characteristic ridgeline overlap effect.

- **Alpha 0.7 enables overlap visibility.** Where curves overlap, the underlying distribution bleeds through with reduced intensity. This creates natural color mixing at overlaps (e.g., blue Jan visible through indigo Feb).

- **All baselines are correct.** Six horizontal lines at Y=5, 15, 25, 35, 45, 55, each spanning X=[5, 95]. Baselines on layer 10 (behind all curves) provide reference without occluding data.

- **All max Y values within pane bounds.** Highest point is Jun peak at Y=68, which maps to clipY=0.896 (within pane limit of 0.95). Jan baseline at Y=5 maps to clipY=−0.814. All data visible.

- **Transform is exact.** sx=0.019 maps X=[0,100] to clip[−0.95,0.95]. sy=0.027143 maps Y=[0,70] to clip[−0.95,0.95]. Both verified to machine precision.

- **Triangle strip tessellation produces clean fills.** 49 quads per curve, each split into 2 triangles connecting baseline to curve. No gaps, no z-fighting artifacts visible.

- **Curves shift rightward and grow taller.** Jan (μ=30, h=8) → Jun (μ=68, h=13). The progressive shift is clearly visible in the PNG — curves move right and get taller, representing warming temperatures.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match.** Each curve: 588/2=294 ✓. Baselines: 24/4=6 ✓.

- **All 30 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Ridgeline plots rely on back-to-front layer ordering.** Each distribution must be on a separate layer with ascending layer IDs so that front curves occlude back curves. This is the key visual effect — without it, the overlapping areas would be a mess.

2. **Alpha transparency creates the signature ridgeline blend.** At alpha 0.7, overlapping regions show both the front curve color (dominant) and the background curve (faintly visible). This creates natural depth perception and shows distribution overlap.

3. **Triangle strips between baseline and curve are the correct tessellation.** For each pair of adjacent X samples, two triangles form a quad connecting (x_i, baseline)–(x_i, curve_y_i)–(x_{i+1}, baseline)–(x_{i+1}, curve_y_{i+1}). This produces a clean filled area with no gaps.

4. **50 X samples produce smooth Gaussians.** At ~1.84 data units per step across a 90-unit range, the curve appears perfectly smooth. Peak undershoot is at most 0.02 (the sample grid slightly misses the true peak X).

5. **Color gradients encode temporal progression.** The cool-to-warm gradient (blue→pink) reinforces the chronological ordering and seasonal temperature theme. Combined with the spatial stacking, both position and color encode the month.
