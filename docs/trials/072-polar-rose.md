# Trial 072: Polar Rose Garden

**Date:** 2026-03-12
**Goal:** 6 rhodonea curves (polar roses) with k values 2, 3, 4, 5, 5/3, and 7 in a 3×2 grid. Tests polar-to-Cartesian coordinate conversion, varying θ ranges for different k types (integer even, integer odd, rational), multi-pane layout, and the k=5/3 non-integer rose pattern on a 900×600 viewport.
**Outcome:** All 5,200 segments across 6 curves match the polar rose formula with 0.000000000 error. Petal counts visually correct (4, 3, 8, 5, complex, 7). All 6 panes non-overlapping. 36 unique IDs. Zero defects.

---

## What Was Built

A 900×600 viewport with 6 panes in a 3×2 grid (background #0f172a each):

**6 polar rose curves (6 lineAA@1 DrawItems, rect4):**

| Position | k | Petals | θ range | Segments | Color |
|----------|---|--------|---------|----------|-------|
| (0,0) | 2 | 4 | [0, 2π] | 800 | #ef4444 (red) |
| (1,0) | 3 | 3 | [0, π] | 800 | #3b82f6 (blue) |
| (2,0) | 4 | 8 | [0, 2π] | 800 | #10b981 (emerald) |
| (0,1) | 5 | 5 | [0, π] | 800 | #f59e0b (amber) |
| (1,1) | 5/3 | complex | [0, 6π] | 1200 | #a855f7 (purple) |
| (2,1) | 7 | 7 | [0, π] | 800 | #06b6d4 (cyan) |

Formula: r(θ) = cos(k·θ), x = r·cos(θ), y = r·sin(θ).

All curves: lineWidth 1.5, alpha 0.8. Total: 5,200 segments, 20,800 floats.

Grid layout: cell 0.6133×0.9300 clip units, gap 0.02. Panes are taller than wide due to 3:2 viewport aspect.

Per-pane transforms: map [-1.1, 1.1] × [-1.1, 1.1] to each pane's clip region.

Total: 36 unique IDs (6 panes, 6 layers, 6 transforms, 6 buffers, 6 geometries, 6 drawItems).

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

- **All 5,200 segments match the polar rose formula exactly.** Every segment verified against r = cos(k·θ) with polar-to-Cartesian conversion. Maximum error: 0.000000000 across all 6 curves.

- **Correct θ ranges for each k type.** Even integer k (2, 4): θ ∈ [0, 2π] traces the complete pattern. Odd integer k (3, 5, 7): θ ∈ [0, π] suffices. Rational k=5/3: θ ∈ [0, 6π] (LCM of denominator × π) required for closure. Each curve traces its complete pattern without redundant retracing.

- **Petal counts match theory.** k=2: 4 petals (2k for even). k=3: 3 petals (k for odd). k=4: 8 petals. k=5: 5 petals. k=7: 7 petals. k=5/3: complex overlapping pattern with 5 primary lobes visible.

- **k=5/3 rose has correct fractal-like structure.** The non-integer k produces overlapping loops that don't simply form distinct petals. The 1200 segments (vs 800 for integer k) provide adequate detail for the 6π parameter range.

- **All 6 panes non-overlapping.** Grid cells 0.6133×0.9300 with 0.02 gaps verified.

- **Pane dimensions accommodate the 3:2 viewport aspect.** Taller cells (0.93 vs 0.61) prevent the roses from being squished, since each pane's transform maps the same [-1.1, 1.1] range to different clip dimensions.

- **Six distinct colors clearly differentiate the curves.** Red, blue, emerald, amber, purple, cyan — each immediately identifiable.

- **All buffer sizes match vertex counts.** 6/6 geometries verified (rect4: 4 fpv).

- **All 36 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Polar rose petal count: k petals for odd integer k, 2k for even.** This is because even-k roses trace each petal twice in [0, 2π], while odd-k roses complete in just [0, π].

2. **Non-integer k requires θ range = lcm(numerator, denominator) × π / denominator.** For k=p/q in lowest terms, the curve closes after q·π radians (if p+q is odd) or 2q·π (if p+q is even). k=5/3: (5+3)=8 is even, so 2×3×π = 6π.

3. **More segments needed for longer θ ranges.** k=5/3 with θ ∈ [0, 6π] needs proportionally more segments (1200 vs 800) to maintain the same angular resolution per segment.

4. **3×2 grid on a 3:2 viewport produces tall rectangular panes.** This is fine for circular patterns (roses) since they're centered and don't fill the full pane width/height. But for patterns that should be square, this would introduce distortion.

5. **Polar-to-Cartesian conversion is straightforward for line segments.** Each segment connects (r1·cos(θ1), r1·sin(θ1)) to (r2·cos(θ2), r2·sin(θ2)). No special handling needed at r=0 crossings — the line segment simply passes through the origin.
