# Trial 091: Inventory Stacked Area

**Date:** 2026-03-22
**Goal:** 3-series stacked area chart showing inventory levels over 20 time periods.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x500 viewport with one pane and three layers.

Three stacked area fills using triSolid@1 triangle strips:
- **Bottom** (green) -- Base inventory, 10-25 units
- **Middle** (blue) -- Secondary stock, 14-30 units added
- **Top** (orange) -- Primary stock, 20-42 units added

Each area has 19 segments (38 triangles). Series are stacked so each layer fills from the previous layer's top to its own cumulative total.

Total: 12 unique IDs (1 pane, 3 layers, 1 transform, 3×(buf+geo+di)=9)

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
- **Stacking math.** Bottom fills 0→s3, middle fills s3→s3+s2, top fills s3+s2→s3+s2+s1.
- **Layer order.** Bottom (layer 10) behind middle (11) behind top (12) ensures correct overlap.
- **Triangle strip correctness.** Two triangles per segment form a watertight quad strip.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Stack from bottom up.** Build cumulative sums and fill between adjacent layers.
2. **Use separate layers for overlapping area fills.** Layer ordering prevents z-fighting.
