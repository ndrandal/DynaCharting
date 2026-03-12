# Trial 029: Bullet Chart

**Date:** 2026-03-12
**Goal:** Five horizontal bullet charts stacked vertically (Revenue, Profit, New Customers, Satisfaction, Order Size), each with 3 qualitative range bands at different alpha levels, a colored actual-value bar, and a white target marker line. Tests instancedRect@1 with overlapping bars at different heights, lineAA@1 for vertical markers, 5-layer depth ordering, and multi-row vertical stacking with consistent gaps.
**Outcome:** All 15 range bands, 5 actual bars, and 5 target markers are exact. Layer ordering, colors, row spacing, and transform all verified. Zero defects.

---

## What Was Built

A 900×600 viewport with a single pane (750×520px, clipX [−0.733, 0.933], clipY [−0.900, 0.833], background #111827):

**5 metrics at Y rows 1–5 (bottom to top: Revenue, Profit, New Customers, Satisfaction, Order Size):**

| Metric | Poor [0,X] | Satisfactory [0,Y] | Good [0,Z] | Actual | Target |
|--------|-----------|-------------------|------------|--------|--------|
| Revenue | 0–25 | 0–50 | 0–75 | 68 | 72 |
| Profit | 0–20 | 0–45 | 0–70 | 55 | 60 |
| New Customers | 0–30 | 0–55 | 0–80 | 72 | 65 |
| Satisfaction | 0–35 | 0–60 | 0–85 | 78 | 80 |
| Order Size | 0–15 | 0–40 | 0–65 | 42 | 50 |

**3 band DrawItems (instancedRect@1, rect4, 5 instances each):**
- Good bands (layer 10): white, alpha 0.12. Full row height (±0.4).
- Satisfactory bands (layer 11): white, alpha 0.20.
- Poor bands (layer 12): white, alpha 0.30.

**5 actual-value bar DrawItems (instancedRect@1, rect4, 1 instance each, layer 13):**
Revenue #3b82f6 (blue), Profit #10b981 (emerald), New Customers #f59e0b (amber), Satisfaction #8b5cf6 (violet), Order Size #ec4899 (pink). Narrower than bands (±0.24 = 60% of band height).

**1 target marker DrawItem (lineAA@1, rect4, 5 instances, layer 14):**
White, alpha 0.9, lineWidth 2. Vertical lines at each metric's target value.

Row layout: band height 0.8 units (69.3px), bar height 0.48 units (41.6px), inter-row gap 0.2 units (17.3px).

Data space: X=[0, 100], Y=[0, 6]. Transform: sx=0.016667, sy=0.288889, tx=−0.733333, ty=−0.900.

Layers: good bands (10) → satisfactory bands (11) → poor bands (12) → actual bars (13) → target markers (14).

Text overlay: title, subtitle, 5 metric labels, 5 actual values, 5 target values, 5 axis scale labels = 22 labels.

Total: 1 pane, 5 layers, 1 transform, 9 buffers, 9 geometries, 9 drawItems = 34 IDs.

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

- **All 15 range bands are exact.** 5 good bands, 5 satisfactory bands, 5 poor bands — every rect4 matches (0, row−0.4, limit, row+0.4) for the correct limit value. Zero errors.

- **All 5 actual-value bars are exact.** Each bar spans (0, row−0.24, actual, row+0.24). The 60% height ratio (0.48/0.8) creates a visually distinct narrower bar centered within the bands.

- **All 5 target markers are exact.** Vertical lines at (target, row−0.4, target, row+0.4) for each metric. The markers span the full band height.

- **5-layer depth ordering is correct.** Good (widest, lightest) → satisfactory → poor (narrowest, darkest) → actual bars → target markers. The layered bands create a subtle stepped gradient effect behind the colored bar.

- **Band alpha progression creates clear visual hierarchy.** Alpha 0.12 (good) → 0.20 (satisfactory) → 0.30 (poor). Since bands overlap (all start at X=0), the poor region appears brightest (cumulative alpha from all 3 bands), satisfactory medium, and good lightest. This naturally encodes "poor = most attention-grabbing."

- **All 5 colors match spec exactly.** Revenue blue, Profit emerald, New Customers amber, Satisfaction violet, Order Size pink — all hex-to-float conversions verified to ≤0.001 precision.

- **Transform is exact.** X=0→clipX=−0.733, X=100→clipX=0.933. Y=0→clipY=−0.900, Y=6→clipY=0.833. The pane region matches these bounds.

- **Row spacing is consistent and adequate.** Each row is 0.8 data units tall with 0.2 units gap = 17.3px separation. Rows are clearly distinct in the rendered image.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓ (bands and bars), lineAA@1 uses rect4 ✓ (markers).

- **All vertex counts match.** Bands: 20/4=5 ✓ (×3). Bars: 4/4=1 ✓ (×5). Markers: 20/4=5 ✓.

- **Text labels are well-positioned.** Metric names right-aligned at clipX=−0.78 (left margin), actual values at bar ends, target values above markers with ▼ indicator. Axis scale labels (0, 25, 50, 75, 100) along the bottom.

- **All 34 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Bullet charts use overlapping bands, not adjacent ones.** All 3 bands start at X=0 and extend to different limits. The back-to-front layer ordering (good→satisfactory→poor) makes all three visible, with the cumulative alpha creating the stepped shading effect.

2. **Separate DrawItems per alpha level is required.** instancedRect@1 applies one color/alpha to all instances. Since the 3 band types have different alphas (0.12, 0.20, 0.30), they need 3 separate DrawItems.

3. **Separate DrawItems per bar color is required.** Similarly, each colored actual bar needs its own DrawItem since instancedRect@1 uses a single color for all instances.

4. **60% bar height ratio is effective.** The narrower actual bar (±0.24) within the wider bands (±0.4) creates clear visual distinction between the qualitative background and the quantitative foreground.

5. **lineAA@1 vertical markers work well at lineWidth 2.** The white target markers are clearly visible against the darker bands without being too thick. The full band-height span makes them easy to locate for each metric.
