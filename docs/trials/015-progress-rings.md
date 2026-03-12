# Trial 015: Progress Rings

**Date:** 2026-03-12
**Goal:** Five concentric progress rings representing project completion metrics (78%, 95%, 62%, 41%, 15%), each with a dim 360° track and a bright partial fill arc, using triAA@1 for smooth anti-aliased edges. First trial with multiple full-circle arc bands at different radii.
**Outcome:** Flawless execution. All radii, fill angles, fringe widths, and segment counts are mathematically exact. The visual result is clean and professional. Zero defects of any severity.

---

## What Was Built

A 900×900 viewport with a single pane (876×876px, 12px margins):

**5 concentric ring pairs (track + fill), from outermost to innermost:**

| Ring | Metric | % | Fill Segments | Inner r | Outer r | Color |
|------|--------|---|--------------|---------|---------|-------|
| 1 | Overall | 78% | 94 of 120 | 0.82 | 0.90 | Blue #4285f4 |
| 2 | Design | 95% | 114 of 120 | 0.65 | 0.73 | Green #34a853 |
| 3 | Development | 62% | 74 of 120 | 0.48 | 0.56 | Orange #fbbc04 |
| 4 | Testing | 41% | 49 of 120 | 0.31 | 0.39 | Red #ea4335 |
| 5 | Deployment | 15% | 18 of 120 | 0.14 | 0.22 | Purple #9c27b0 |

Each ring has:
- **Track arc** (`triAA@1`, pos2_alpha, 2160 vertices = 120 segments × 18 verts/seg): full 360° circle at alpha 0.15
- **Fill arc** (`triAA@1`, pos2_alpha, proportional segment count): partial arc at alpha 1.0

All arcs start at 90° (12 o'clock) and sweep clockwise. Each segment spans 3°.

Tessellation per segment: 3 bands × 2 triangles × 3 vertices = 18 vertices:
- Outer fringe: r_outer+fringe (alpha=0) → r_outer (alpha=1)
- Core: r_outer (alpha=1) → r_inner (alpha=1)
- Inner fringe: r_inner (alpha=1) → r_inner−fringe (alpha=0)

Gap between rings: 0.09 data units = 39.4px. Ring band width: 0.08 data units = 35.0px (uniform for all 5).

Text overlay: "Project Status" title (white 18px), "78%" center (blue 28px), 5 per-ring labels at the ring's peak height.

Total resources: 1 pane, 2 layers, 1 transform, 10 buffers, 10 geometries, 10 drawItems, 1 viewport = 34 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the rings have no metric names or percentage values. The center "78%" and "Project Status" title are also absent.

---

## Spatial Reasoning Analysis

### Done Right

- **All vertex formats are correct.** All 10 DrawItems use `triAA@1` with `pos2_alpha` format ✓. This is the second trial (after 013) to have zero format mismatches.

- **All 10 ring radii match spec exactly.** Verified at the first vertex of each track: Overall 0.82–0.90, Design 0.65–0.73, Development 0.48–0.56, Testing 0.31–0.39, Deployment 0.14–0.22. All to 4 decimal places.

- **Fringe width is exact.** All rings: outer fringe at r+0.00571, inner fringe at r−0.00571. Expected: FRINGE_PIXELS (2.5) / pane_half_px (438) = 0.00571 ✓. Uniform across all 10 arcs.

- **Fill segment counts are exact.** 78%→94, 95%→114, 62%→74, 41%→49, 15%→18. All match round(pct/100 × 120) ✓.

- **Fill arc end angles are exact.** Verified last-segment vertex angles: Overall ends at 168°, Design at 108°, Development at −132°, Testing at −57°, Deployment at 36°. All match 90° − segments×3° ✓.

- **All arcs start at 90° (12 o'clock).** First vertex of every buffer is at angle 90.0° ✓.

- **Rings are perfectly circular.** Pane is 876×876px (aspect ratio 1.0000). Data range [-1,1]×[-1,1]. Pixel radius is identical in both axes: 0.90 × 438 = 394.2px. No aspect correction needed, and none applied. Verified at 45° (segment 15): x=y=0.64043, confirming circular geometry.

- **Tessellation structure is correct.** Each segment has 3 bands (outer fringe, core, inner fringe) of 2 explicit triangles each. Alpha transitions: 0→1 at outer fringe, 1→1 through core, 1→0 at inner fringe. This produces smooth anti-aliased edges on both inner and outer ring borders.

- **Layer ordering is correct.** Tracks on layer 10 (behind), fills on layer 11 (on top). Fill arcs render over the track, producing the bright-over-dim effect. The unfilled portion of the track remains visible at dim alpha (0.15).

- **Vertex counts match buffer sizes.** All 10 geometries: buffer floats / 3 = declared vertexCount. Zero mismatches.

- **All 34 IDs unique.** Systematic allocation: pane 1, layers 10-11, transform 50, then triplets (buf, geom, draw) from 100-129.

- **Text label positions are well-computed.** Each per-ring label's clipY corresponds to the ring's center radius converted to clip space: Overall 0.86 → 0.837 clip (label at 0.8371), Design 0.69 → 0.672 (label at 0.6716), etc. All match to 3 decimal places. At the ring's peak height, the horizontal extent of the ring is near zero, so clipX=0.6 places labels clearly to the right.

- **Uniform ring proportions.** All 5 rings have identical band width (0.08 data units = 35.0px) and identical inter-ring gap (0.09 data units = 39.4px). This creates a balanced, harmonious visual.

### Done Wrong

Nothing. This is the cleanest trial in the series.

---

## Lessons for Future Trials

1. **Square viewports eliminate aspect correction complexity.** The 900×900 viewport produces a 876×876 pane with aspect ratio 1.0, meaning data-space circles are pixel-space circles with no correction factor. For circular visualizations, prefer square viewports when possible.

2. **Track + fill pattern works cleanly with triAA@1.** Two overlapping arcs — one at reduced alpha (track, full 360°) and one at full alpha (fill, partial) — on separate layers produces the standard progress ring visual. The layer ordering ensures fills render on top.

3. **18 vertices per segment is the triAA@1 arc formula.** For an arc band with AA fringe: 3 bands (outer fringe, core, inner fringe) × 2 triangles × 3 vertices = 18 vertices per angular segment. Total vertices = segments × 18.

4. **This is the first trial with zero defects at any severity** (excluding the known text-in-PNG limitation). The combination of a square viewport, well-specified radii, and systematic tessellation left no room for error.
