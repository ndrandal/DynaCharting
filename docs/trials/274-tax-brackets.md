# Trial 274: Tax Brackets

**Date:** 2026-03-22
**Goal:** 6 tax bracket bars stacked horizontally, widths proportional to rate (10-37%). Green-to-red color gradient progression.
**Outcome:** 6 brackets (10%, 12%, 22%, 24%, 32%, 37%) with proportional widths, divider lines, baseline. 27 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x400. Single pane with dark background.

**8 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102-117 | 10 | 6 bracket bars | instancedRect@1 | 1 each | green-to-red |
| 120 | 11 | Dividers | lineAA@1 | 5 segs | white |
| 123 | 11 | Baseline | lineAA@1 | 1 seg | gray |

Bar widths proportional to tax rate. Total rate sum = 137%. Widest bar = 37% bracket.

Total: 27 unique IDs (1 pane, 2 layers, 8 buffers, 8 geometries, 8 drawItems).

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
- **Widths proportional to rate.** Higher brackets get wider bars, immediately showing their relative impact.
- **Green-to-red gradient progression.** Low rates (10%) in green, high rates (37%) in red — intuitive danger signal.
- **Divider lines mark bracket boundaries.** Thin white lines separate adjacent brackets clearly.
- **Bars fill available horizontal space.** Total width = 1.7 clip units, distributed proportionally.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Proportional width encoding.** Width proportional to value creates a Marimekko-style visualization.
2. **Graduated color scales.** Green-yellow-orange-red progression signals increasing intensity.
