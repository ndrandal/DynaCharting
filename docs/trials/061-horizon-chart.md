# Trial 061: Horizon Chart

**Date:** 2026-03-12
**Goal:** 3-pane horizon chart showing 3 time series with 6-band folding (3 positive blue, 3 negative red), graduated color intensity by magnitude, and triangle-strip fills. Tests data folding math (min/max clamping at band thresholds), multi-pane layout with per-pane transforms, 18 layered triSolid@1 fills, and the horizon chart's spatial compression technique on a 1000×400 viewport.
**Outcome:** All 282 non-zero band values match the folding formula with 0.000000 maximum error. All 18 bands correctly layered (lighter behind darker). All 3 panes correctly positioned. Zero defects.

---

## What Was Built

A 1000×400 viewport with 3 vertically stacked panes (background #0f172a):

**3 panes:**
| Pane | Series | Y Region | Transform |
|------|--------|----------|-----------|
| 1 (top) | CPU Usage | [0.34, 0.98] | sx=0.04, sy=0.64, tx=−0.98, ty=0.34 |
| 2 (mid) | Memory | [−0.32, 0.32] | sx=0.04, sy=0.64, tx=−0.98, ty=−0.32 |
| 3 (bot) | Network | [−0.98, −0.34] | sx=0.04, sy=0.64, tx=−0.98, ty=−0.98 |

**Series formulas (50 data points, X=[0, 49]):**
- CPU: y(x) = 2.5 sin(2πx/25) + 0.3 sin(2πx/7)
- Memory: y(x) = 1.8 sin(2πx/30) + 0.8 cos(2πx/10) + 0.5 sin(2πx/5)
- Network: y(x) = 2.0 sin(2πx/20) + 1.5 sin(2πx/8) cos(2πx/15)

**6 bands per pane (18 triSolid@1 DrawItems total):**

| Band | Folding Formula | Color | Layer Offset |
|------|----------------|-------|--------------|
| −1 | min(max(−v, 0), 1) | #fca5a5 (light red) | +0 (back) |
| −2 | min(max(−v−1, 0), 1) | #ef4444 (medium red) | +1 |
| −3 | min(max(−v−2, 0), 1) | #b91c1c (dark red) | +2 |
| +1 | min(max(v, 0), 1) | #93c5fd (light blue) | +3 |
| +2 | min(max(v−1, 0), 1) | #3b82f6 (medium blue) | +4 |
| +3 | min(max(v−2, 0), 1) | #1d4ed8 (dark blue) | +5 (front) |

Each band: 294 vertices (98 individual triangles from 49 quads between 50 data points). All alpha 0.9.

All 18 bands active across all 3 panes.

Data space per band: X=[0, 49], Y=[0, 1] (after folding). Per-pane transforms map to clip regions.

Layers: Pane 1 (10–15), Pane 2 (20–25), Pane 3 (30–35). Within each pane: lighter bands behind, darker bands in front.

Total: 78 unique IDs.

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

- **All 282 non-zero band values match the folding formula exactly.** Every data vertex height was verified against min(max(v − threshold, 0), 1) for positive bands and min(max(−v − threshold, 0), 1) for negative bands. Maximum error: 0.000000.

- **Graduated color intensity is clearly visible.** The PNG shows light blue/red for small magnitudes, progressively darker for larger magnitudes. The layered bands create the characteristic horizon chart appearance where dark peaks emerge from lighter backgrounds.

- **Layer ordering is correct (lighter behind darker).** Within each pane, band −1 (light red) is on the lowest layer, band −3 (dark red) on the highest of the negative group. Band +1 (light blue) is behind +2 and +3 (dark blue). This means darker bands overlay lighter ones, creating the graduated depth effect.

- **3 panes correctly positioned with gaps.** Pane 1 at [0.34, 0.98], Pane 2 at [−0.32, 0.32], Pane 3 at [−0.98, −0.34]. The 2% gaps (0.02 clip units) between panes create visual separation.

- **Per-pane transforms correctly map data to clip regions.** Each transform maps X=[0,49] to clipX [−0.98, 0.98] (sx=1.96/49=0.04) and Y=[0,1] to the pane's clipY range (sy=0.64 for each pane's 0.64-unit height).

- **Three distinct series create visual variety.** CPU (sine-dominated) produces smooth waves, Memory (multi-frequency) creates complex interference, Network (product of sines) generates spiky patterns. Each pane has a visually distinct character.

- **Band folding produces correct visual compression.** A data range of [−3, 3] is compressed into [0, 1] height per pane through the 6-band folding technique. This 6× vertical compression is the defining feature of horizon charts.

- **Triangle tessellation is correct.** 49 quads × 2 triangles × 3 vertices = 294 vertices per band. Each triangle connects baseline and data vertices correctly.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓.

- **All buffer sizes match vertex counts.** 18/18 geometries verified (294 verts × 2 fpv = 588 floats each).

- **All 78 IDs unique.** No collisions across 3 panes, 18 layers, 3 transforms, 18 buffers, 18 geometries, 18 drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Horizon chart band folding uses min/max clamping.** For band k (1-indexed): positive height = min(max(v − (k−1), 0), 1), negative height = min(max(−v − (k−1), 0), 1). Each band captures exactly 1 unit of magnitude, folded to [0, 1].

2. **Lighter bands behind darker bands creates the graduated effect.** Darker bands are subsets of lighter bands (they only exist where magnitude exceeds a higher threshold). Drawing darker bands on top creates the visual intensity gradient.

3. **Per-pane transforms enable independent Y scaling.** Each pane maps Y=[0, 1] to its own clip region. The same data-space band fills render in different viewport positions through different transforms.

4. **18 DrawItems for 3 panes × 6 bands is manageable.** Each band needs its own DrawItem for color differentiation. The regular ID allocation pattern (base + pane × 6 + band) keeps the namespace organized.

5. **Individual triangles (not strips) for triSolid@1 fills.** The agent used 294 vertices (98 triangles) per band rather than triangle strips. This avoids degenerate triangle issues at zero-height segments where the band has no data.
