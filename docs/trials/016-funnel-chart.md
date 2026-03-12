# Trial 016: Funnel Chart

**Date:** 2026-03-12
**Goal:** Six-stage vertical sales conversion funnel with trapezoid shapes decreasing in width (100% → 68% → 42% → 25% → 14% → 8%), cool-to-warm color progression, and centered labels. First trial with non-rectangular polygon tessellation (trapezoids via triSolid@1).
**Outcome:** Textbook funnel chart. All stage widths match conversion percentages exactly, trapezoid geometry is correct, color progression is effective. Zero major defects.

---

## What Was Built

An 800×900 viewport with a single pane (776×876px, 12px margins):

**6 funnel stages (triSolid@1, pos2_clip, 6 vertices each = 2 triangles):**

| Stage | Metric | % | Top Width | Bot Width | Height | Color |
|-------|--------|---|-----------|-----------|--------|-------|
| 1 | Visitors | 100% | 646.7px | 439.7px | 103.4px | Blue |
| 2 | Leads | 68% | 439.7px | 271.6px | 103.4px | Teal |
| 3 | Qualified | 42% | 271.6px | 161.7px | 103.4px | Green |
| 4 | Proposals | 25% | 161.7px | 90.5px | 103.4px | Orange |
| 5 | Negotiations | 14% | 90.5px | 51.7px | 103.4px | Red-orange |
| 6 | Closed Won | 8% | 51.7px | 51.7px | 103.4px | Red |

Each stage's top edge matches the previous stage's bottom edge width, creating the continuous funnel taper. The final stage is a rectangle (equal top and bottom width).

Gap between stages: 18.2px (0.15 data units).

Data space: [-0.6, 0.6] × [-0.1, 7.1]. Transform: sx=1.6167, sy=0.2704, tx=0, ty=-0.9463.

Text overlay: "Sales Funnel" title (18px), 6 per-stage labels centered vertically on each trapezoid.

Total resources: 1 pane, 1 layer, 1 transform, 6 buffers, 6 geometries, 6 drawItems, 1 viewport = 21 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, stages are identified only by color and position. The percentage annotations are absent.

2. **Inter-stage gaps are 18px, not ~6px.** The spec estimated 0.15 data units would map to ~6px, but with the actual viewport scale (7.2 data units → 876px), 0.15 data units = 18.2px. This is a spec estimation error — the agent correctly used the specified data coordinates. The 18px gaps are visually acceptable and clearly separate each stage.

---

## Spatial Reasoning Analysis

### Done Right

- **All stage widths match conversion percentages exactly.** Verified via pixel widths relative to the Visitors stage (646.7px = 100%): Leads 439.7/646.7 = 68.0%, Qualified 271.6/646.7 = 42.0%, Proposals 161.7/646.7 = 25.0%, Negotiations 90.5/646.7 = 14.0%, Closed Won 51.7/646.7 = 8.0%. All match to 3 significant figures.

- **Trapezoid continuity is perfect.** Each stage's bottom half-width equals the next stage's top half-width: ±0.50 → ±0.34 → ±0.21 → ±0.125 → ±0.07 → ±0.04. No visual seams between adjacent stage edges.

- **All vertex formats are correct.** All 6 DrawItems use `triSolid@1` with `pos2_clip` format ✓. 12 floats per buffer, 6 vertices per geometry, all verified.

- **All stage heights are uniform.** Each stage spans 0.85 data units (e.g., 6.00 to 5.15), producing 103.4px of height. The equal heights give a clean stacked appearance.

- **All gaps are uniform.** 0.15 data units between each pair = 18.2px consistently. The dark stripes visually separate the stages without being distracting.

- **All 6 label positions are exact.** Each per-stage label clipY matches the stage's vertical center to 3 decimal places: Visitors at 0.561, Leads at 0.291, Qualified at 0.020, Proposals at -0.250, Negotiations at -0.520, Closed Won at -0.791. All verified against computed midpoints.

- **Color progression is coherent.** Cool colors (blue, teal, green) at the wide top transition to warm colors (orange, red-orange, red) at the narrow bottom. This matches the funnel metaphor: abundance → scarcity.

- **The final stage is a correct rectangle.** Both top and bottom half-widths are ±0.04, confirming the funnel terminates cleanly rather than tapering to a point.

- **All 21 IDs unique.** Pane 1, layer 10, transform 50, then triplets (100-117). No collisions.

- **Transform is correctly derived.** The viewport maps data [-0.6, 0.6] × [-0.1, 7.1] to pane clip [-0.97, 0.97] × [-0.97333, 0.97333], producing sx=1.6167, sy=0.2704. The asymmetric scale factors correctly handle the non-square data range in the non-square pane.

### Done Wrong

Nothing structurally wrong. The gap-size estimate in the spec was inaccurate, but the agent's implementation of the specified data coordinates was correct.

---

## Lessons for Future Trials

1. **Trapezoid tessellation is trivial with triSolid@1.** Two triangles (6 vertices) per trapezoid, no special tessellation needed. Each stage is independently colored via its DrawItem. This scales easily to any number of stages.

2. **Funnel charts are resource-efficient.** 21 IDs for 6 data stages — the simplest non-trivial trial since 011 (waterfall, 20 IDs). The key: one layer suffices since stages don't overlap.

3. **Verify data-unit-to-pixel estimates before specifying.** The spec claimed 0.15 data units ≈ 6px, but the actual mapping was 18px. Always compute: gap_px = gap_data × (pane_px / data_range). Here: 0.15 × (876 / 7.2) = 18.3px.

4. **Portrait viewports work well for vertical funnels.** The 800×900 aspect gives the funnel adequate horizontal spread at the top while providing vertical space for 6 stages + gaps + margins.
