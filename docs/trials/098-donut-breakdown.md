# Trial 098: Donut Breakdown

**Date:** 2026-03-22
**Goal:** Donut chart with 5 sectors and center hole showing department budget allocation.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

700x700 viewport with one pane.

Five donut sectors with inner radius 0.35 and outer radius 0.75:
Engineering (30%, blue), Marketing (22%, orange), Sales (20%, green), Support (15%, yellow), Admin (13%, purple). Each sector uses ring_arc tessellation (24 segments, 48 triangles per sector). Center hole reveals the dark background.

Total: 18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)

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
- **Sector angles.** Fractions sum to 1.0, sectors are contiguous with no gaps.
- **Ring tessellation.** Inner/outer radius quads prevent the center from filling.
- **Proportional sizing.** Largest sector (Engineering 30%) subtends 108° of arc.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use ring_arc_tris for donut charts.** The inner/outer radius approach avoids a separate clip mask.
2. **Verify fractions sum to 1.0.** 0.30+0.22+0.20+0.15+0.13 = 1.00.
