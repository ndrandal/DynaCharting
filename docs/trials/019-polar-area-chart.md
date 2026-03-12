# Trial 019: Polar Area Chart (Coxcomb/Nightingale)

**Date:** 2026-03-12
**Goal:** Twelve-month polar area chart with equal 30° sectors of varying radii proportional to monthly website traffic, seasonal color cycle, and 1° inter-sector gaps. First trial with variable-radius sector tessellation (Nightingale/Coxcomb style).
**Outcome:** Visually striking and mathematically exact. All 12 radii, angular spans, gaps, and label positions verified. Zero defects of any severity.

---

## What Was Built

A 900×900 viewport with a single pane (876×876px, 12px margins):

**12 monthly sectors (triAA@1, pos2_alpha, 90 vertices each):**

| Month | Traffic (K) | Radius | Pixel r | Start° | End° | Color |
|-------|------------|--------|---------|--------|------|-------|
| Jan | 45 | 0.2942 | 132.4px | 89.5 | 60.5 | Blue |
| Feb | 52 | 0.3400 | 153.0px | 59.5 | 30.5 | Light blue |
| Mar | 78 | 0.5100 | 229.5px | 29.5 | 0.5 | Teal |
| Apr | 95 | 0.6212 | 279.5px | −0.5 | −29.5 | Green |
| May | 110 | 0.7192 | 323.6px | −30.5 | −59.5 | Yellow-green |
| Jun | 125 | 0.8173 | 367.8px | −60.5 | −89.5 | Orange-yellow |
| Jul | 130 | 0.8500 | 382.5px | −90.5 | −119.5 | Orange |
| Aug | 118 | 0.7715 | 347.2px | −120.5 | −149.5 | Orange |
| Sep | 92 | 0.6015 | 270.7px | −150.5 | −179.5 | Brown |
| Oct | 68 | 0.4446 | 200.1px | 179.5 | 150.5 | Purple |
| Nov | 55 | 0.3596 | 161.8px | 149.5 | 120.5 | Indigo |
| Dec | 48 | 0.3138 | 141.2px | 119.5 | 90.5 | Blue |

Radius formula: value / 130 × 0.85 (normalized to max value, scaled to 85% of clip range).

Each sector: 30 core vertices (triangle fan from origin, alpha=1.0) + 60 fringe vertices (outer arc, alpha 1→0). 10 angular segments per sector (2.9° each within 29° active span).

All sectors start at 12 o'clock (90°), sweep clockwise. 1° gaps between all adjacent sectors.

Text overlay: "Monthly Traffic" title, "Visitors (thousands)" subtitle, 12 month abbreviations at radius 0.92.

No transform or viewport defined — vertex data is in clip-space coordinates directly.

Total resources: 1 pane, 1 layer, 12 buffers, 12 geometries, 12 drawItems = 38 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the months and traffic values are absent. The seasonal color cycle is the only month-identification cue.

2. **No transform or viewport defined.** The chart works because vertex data is in clip-space coordinates (all within [-1, 1]), but this deviates from the SceneDocument convention where viewports define the data-to-clip mapping. The engine defaults to identity transform, rendering correctly.

---

## Spatial Reasoning Analysis

### Done Right

- **All 12 radii match exactly.** Jan 0.2942, Feb 0.3400, ..., Jul 0.8500, ..., Dec 0.3138. All verified against value/130 × 0.85 to 4 decimal places. Oct and Nov have negligible float precision variation (±0.00001) across vertices — within machine epsilon.

- **All 12 sectors span exactly 29.0°.** Every sector covers exactly 29° of arc (30° slot minus 1° gap). No angular truncation or overlap.

- **All 11 inter-sector gaps are exactly 1.0°.** The gap between each sector's last perimeter vertex and the next sector's first perimeter vertex is uniformly 1.0° — verified for all 11 boundaries. The gap between Dec's end (90.5°) and Jan's start (89.5°) completes the 1° gap for the full circle.

- **All vertex formats correct.** All 12 DrawItems use `triAA@1` with `pos2_alpha` ✓. Zero format mismatches.

- **Fringe width is close to target.** Core radius → fringe radius offset = 0.00571 data/clip units ≈ 2.57px. Expected: 2.5px. The 0.07px difference (agent used pane half-width 438px instead of viewport half-width 450px for the fringe calculation) is imperceptible.

- **Sectors are circular.** Square pane (876×876px) ensures aspect ratio 1.0. No X/Y radius correction needed.

- **All 12 label positions are exact.** Month abbreviations positioned at radius 0.92 × (cos(θ), sin(θ)) where θ is the sector's center angle. All 12 verified to 3 decimal places.

- **Color progression creates a seasonal narrative.** Blue (winter) → green (spring) → orange (summer) → purple (autumn) → blue (winter). The warm colors correspond to the largest sectors (high traffic months), reinforcing the visual message.

- **All 38 IDs unique.** Systematic triplet allocation (100-135) with no collisions.

- **Tessellation structure is clean.** 30 core vertices (10 triangles, center to arc, alpha=1) + 60 fringe vertices (10 quad segments, alpha 1→0). Total 90 vertices per sector × 12 = 1,080 total vertices.

### Done Wrong

Nothing structurally wrong. The missing viewport/transform is unconventional but functionally correct for a static chart in clip-space coordinates.

---

## Lessons for Future Trials

1. **Polar area charts work well with per-sector triAA@1 DrawItems.** Each sector has its own radius, color, and vertex buffer. 12 DrawItems is manageable. The variable-radius feature (vs. equal-radius pie chart) adds a powerful data-encoding dimension.

2. **The 1° gap technique clearly separates sectors.** Unlike pie charts where sectors are flush, the angular gaps create visible dark lines between sectors, making each month's contribution individually identifiable.

3. **Clip-space vertex data eliminates the need for transforms.** For static charts centered at origin with data in [-1, 1], writing directly in clip space avoids transform setup. This is simpler but sacrifices the ability to add pan/zoom later.

4. **Nightingale charts create striking seasonal patterns.** The variation in radius across months produces a distinctive asymmetric flower shape — small winter petals at top, large summer petals at bottom. This is immediately visually interpretable even without labels.
