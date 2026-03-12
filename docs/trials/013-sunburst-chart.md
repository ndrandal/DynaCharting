# Trial 013: Sunburst Chart

**Date:** 2026-03-12
**Goal:** Two-level sunburst (concentric ring) chart showing departmental budget allocation with 5 inner sectors and 13 outer sub-sectors, all aspect-corrected for circularity. First trial to combine donut-style arc tessellation with hierarchical data layout.
**Outcome:** Most visually striking trial yet. All formats correct (finally!), rings are perfectly circular, sector proportions exact, sub-sectors aligned with parents. Zero major defects.

---

## What Was Built

A 1000×800 viewport with a single pane (980×768px):

**Inner ring** (5 department sectors, `triSolid@1`, y radii 0.3325–0.6175):
- Engineering 35% (blue #4285f4, 252 verts = 42 segments)
- Marketing 25% (green #34a853, 180 verts = 30 segments)
- Operations 20% (orange #fbbc04, 144 verts = 24 segments)
- Sales 15% (red #ea4335, 108 verts = 18 segments)
- R&D 5% (purple #9c27b0, 36 verts = 6 segments)

**Outer ring** (13 sub-category sectors, `triSolid@1`, y radii 0.665–0.9025):
- Engineering: Frontend (84v), Backend (108v), DevOps (60v) — lighter blues
- Marketing: Digital (72v), Brand (60v), Events (54v) — lighter greens
- Operations: IT (66v), Facilities (42v), HR (36v) — lighter yellows
- Sales: Enterprise (66v), SMB (42v) — lighter reds
- R&D: Research (24v), Patents (18v) — lighter purples

Aspect correction: 768/980 = 0.7837. Outer pixel radius: 288.8px in both axes.
1° angular gaps between all sectors. Sectors start at 12 o'clock, proceed clockwise.

Total: 1 pane, 2 layers, 1 transform, 18 buffers, 18 geometries, 18 drawItems, 1 viewport = 58 IDs, 1452 vertices.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, departments are identified only by color. The percentage breakdown is not visible.

2. **R&D sector is very thin.** At 5% of 360° ≈ 18°, the inner R&D sector and its two sub-sectors (Research 3%, Patents 2%) are narrow slivers. The purple is visible but the two outer sub-sectors are barely distinguishable. This is accurate to the data — not a bug — but the thin sectors reduce readability.

---

## Spatial Reasoning Analysis

### Done Right

- **All vertex formats are correct.** All 18 DrawItems use `triSolid@1` with `pos2_clip` format ✓. This is the first trial since the format-mismatch issue began in trial 009 to have zero format mismatches (though this trial doesn't use lineAA@1, so the lineAA format question doesn't arise).

- **Rings are perfectly circular.** Outer ring pixel radii: X = 0.7073 × 408.3 = 288.8px, Y = 0.9025 × 320.0 = 288.8px. Match within 0.1px. Aspect correction factor 0.7837 correctly compensates for the 980×768px pane.

- **Sector proportions are exact.** Inner ring segment counts: 42:30:24:18:6 = 7:5:4:3:1 = 35:25:20:15:5 ✓. The segment count is proportional to each department's budget share, ensuring visually accurate angular spans.

- **Sub-sectors align with parents.** Each group of outer sectors (e.g., 3 blue sub-sectors) is angularly bounded within its parent's inner sector span. The gaps between outer sectors within a department are smaller than the gaps at department boundaries, creating visual grouping.

- **Color hierarchy is clear.** Inner sectors use saturated brand colors; outer sub-sectors use lighter variants of the same hue. The parent-child relationship is immediately apparent from the color progression.

- **Gap between rings is visible.** The 0.0475 data-unit gap between inner ring outer radius (0.6175) and outer ring inner radius (0.665) creates a ~15px dark band that clearly separates the two levels.

- **All 58 IDs unique.** Century-range pattern with 2 layers (10 for inner, 20 for outer), systematic buffer/geometry/drawItem allocation.

### Done Wrong

- Nothing structurally wrong. The only limitation is the inherent thinness of small-percentage sectors.

---

## Lessons for Future Trials

1. **Sunburst charts are achievable with `triSolid@1` per sector.** Each sector is independently colored via its DrawItem's color property. The overhead of 18 DrawItems is manageable. For a 3-level sunburst, this approach would scale to ~50+ DrawItems.

2. **Hierarchical angular layout requires careful bookkeeping.** The outer ring's sub-sectors must fit within the parent's angular range. The approach: compute inner sector angles first, then subdivide each inner sector's range among its children (minus inter-child gaps).

3. **Two layers suffice for two ring levels.** All inner sectors share layer 10, all outer sectors share layer 20. The layer determines draw order (inner first, outer on top), and within a layer, sectors don't overlap so drawing order is irrelevant.

4. **Small-percentage sectors need minimum angular spans.** At 2% (Patents), the sector spans ~7°. Below ~5° a sector becomes a barely-visible sliver. Consider merging very small sectors into an "Other" category or enforcing a minimum angular span.
