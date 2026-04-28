# Trial 277: All Features Showcase

**Date:** 2026-03-22
**Goal:** ULTIMATE engine capabilities demo — all 8 drawable pipelines, all 4 blend modes, both gradient types, stencil clipping, dashed lines, cornerRadius, multiple panes, multiple layers, transforms, varying pointSize and lineWidth.
**Outcome:** 20 DrawItems across 2 panes and 7 layers. 8 pipelines, 4 blend modes, linear + radial gradients, stencil clip pair, dashed lines, cornerRadius, 2 transforms. 71 unique IDs. Zero defects.

---

## What Was Built

Viewport 1000x700. Two panes (top showcase + bottom candle chart).

**Feature Checklist:**

| Feature | Present | DrawItem(s) |
|---------|---------|-------------|
| triSolid@1 | YES | 102 (star), 144 (clip source), blend circles |
| triAA@1 | YES | 105 (AA circle) |
| triGradient@1 | YES | 108 (RGB triangle) |
| line2d@1 | YES | 111 (zigzag) |
| lineAA@1 | YES | 114 (arc), 153/156/159 (width demo) |
| points@1 | YES | 117 (scatter), 150 (transformed ring) |
| instancedRect@1 | YES | 120 (bars), 138/141 (gradients), 147 (clipped) |
| instancedCandle@1 | YES | 123 (15 candles) |
| blendMode: normal | YES | 126 |
| blendMode: additive | YES | 129 |
| blendMode: multiply | YES | 132 |
| blendMode: screen | YES | 135 |
| gradient: linear | YES | 138 (45-degree blue-to-pink) |
| gradient: radial | YES | 141 (yellow-to-purple) |
| stencil clip (isClipSource) | YES | 144 (circle mask) |
| stencil clip (useClipMask) | YES | 147 (rect clipped to circle) |
| dashed lines | YES | 114 (dashLength=0.05) |
| cornerRadius | YES | 120 (bars, r=3) |
| multiple panes | YES | 2 panes |
| multiple layers | YES | 7 layers |
| transforms | YES | 50 (candle), 51 (point ring) |
| varying pointSize | YES | 117 (6.0), 150 (8.0) |
| varying lineWidth | YES | 153 (1.0), 156 (3.0), 159 (6.0) |

**20 DrawItems across 2 panes:**

Pane 1 (Showcase): Star (triSolid), AA circle (triAA), RGB triangle (triGradient), zigzag (line2d), dashed arc (lineAA), scatter (points), bar chart (instancedRect), 4 blend-mode circles, 2 gradient rects, stencil clip pair, transformed point ring, 3 line-width demos.

Pane 2 (Candles): 15 candlesticks with transform, colorUp/colorDown.

Total: 71 unique IDs (2 panes, 7 layers, 2 transforms, 20 buffers, 20 geometries, 20 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **All 8 pipelines exercised.** Every drawable pipeline type has at least one representative DrawItem.
- **All 4 blend modes demonstrated.** Four colored circles side by side, each with a different blend mode — visually comparable.
- **Both gradient types present.** Linear (45-degree blue-to-pink) and radial (yellow center to purple edge) on adjacent rects.
- **Stencil clipping works as circle mask.** A circle clip source followed by a clipped rect produces a circular window effect.
- **Dashed arc demonstrates dash+gap.** dashLength=0.05 and gapLength=0.025 on a 270-degree arc.
- **Three line widths demonstrate scaling.** 1.0, 3.0, and 6.0 px lines side by side for comparison.
- **Two panes with different content.** Showcase pane above candle pane, each with its own background and layers.
- **Two transforms with different values.** Transform 50 for candle data mapping, transform 51 for point ring offset.
- **cornerRadius on bar chart.** Rounded tops on instancedRect bars.
- **Varying pointSize.** Scatter at 6.0px, transformed ring at 8.0px.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Feature showcase requires careful spatial planning.** Each feature needs its own visual space to avoid overlap and confusion.
2. **Blend modes are best compared side-by-side.** Same shape, same alpha, different blend mode makes differences visible.
3. **Stencil clip requires two DrawItems.** One for the mask (isClipSource), one for the content (useClipMask), on adjacent layers.
4. **instancedCandle@1 in a separate pane.** Candles need their own transform (data space mapping), so a separate pane with its own Y range works best.
