# Trial 032: Spiral Timeline

**Date:** 2026-03-12
**Goal:** Archimedean spiral with 24 color-coded event markers across 3 full turns. Tests parametric curve tessellation (180 lineAA@1 segments), triAA@1 circles on polar-to-Cartesian positions, spiral continuity, and square-viewport aspect correction (trivial case: ratio=1.0).
**Outcome:** All 180 spiral segments are exact and continuous (zero breaks). All 24 event positions match parametric formula to ≤0.001 precision. Circle radii exactly 5.000px. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (clipX [−0.95, 0.95], clipY [−0.95, 0.95], background #0f172a):

**Archimedean spiral:** r(θ) = 0.5 + 0.35·θ, θ ∈ [0, 6π] (3 full turns).

**180 spiral segments (1 lineAA@1 DrawItem, rect4, 180 instances):**
White, alpha 0.25, lineWidth 1. Tessellated at Δθ = π/30 (~6° per segment). Max radius 7.097 at θ=6π.

**24 event dots across 4 groups (4 triAA@1 DrawItems, pos2_alpha, 864 vertices each = 6 circles × 144):**

| Group | Events | Color | Layer |
|-------|--------|-------|-------|
| Q1 | 0–5 | #3b82f6 (blue) | 11 |
| Q2 | 6–11 | #10b981 (emerald) | 12 |
| Q3 | 12–17 | #f59e0b (amber) | 13 |
| Q4 | 18–23 | #ec4899 (pink) | 14 |

Events evenly spaced: θ_i = i × 6π/23. Positions: x = r(θ)·cos(θ), y = r(θ)·sin(θ).

Circle radii: X=Y=0.120301 data units. Pixel radius: 5.000px both axes. Aspect ratio 1.0 (square viewport, symmetric data range, square pane). Fringe: 2.5px.

Data space: X=[−8, 8], Y=[−8, 8]. Transform: sx=sy=0.11875, tx=ty=0 (origin-centered, symmetric).

Layers: spiral (10) → Q1 (11) → Q2 (12) → Q3 (13) → Q4 (14).

Text overlay: title, subtitle, 4 legend entries = 6 labels.

Total: 1 pane, 5 layers, 1 transform, 5 buffers, 5 geometries, 5 drawItems = 22 IDs.

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

- **All 24 event positions are exact.** Each event's center matches the parametric formula r(θ)·(cos θ, sin θ) to ≤0.001 data units. Verified all 24 events across all 4 groups.

- **All 180 spiral segments are exact.** Spot-checked segments at θ = 0, 3π/2, 3π, 9π/2, and near 6π — all match the parametric formula precisely.

- **Spiral is perfectly continuous.** All 179 segment-to-segment transitions verified: end of segment i = start of segment i+1. Zero continuity breaks.

- **Spiral starts and ends correctly.** First point at (0.5, 0) (θ=0, r=0.5). Last point at (7.097, −0.000) (θ=6π, r=7.097). The spiral completes exactly 3 full turns.

- **Circles are perfectly circular at 5px.** With aspect ratio 1.0 (square viewport, square pane, symmetric data range), X and Y data radii are equal (0.120301). Verified at 0° and 90° — both yield exactly 5.000px.

- **Transform is optimally simple.** sx=sy=0.11875, tx=ty=0. The origin-centered symmetric data range maps to a centered pane with no offset. This is the ideal transform for a radially symmetric visualization.

- **Color groups progress outward along the spiral.** Q1 (blue) occupies the innermost turn (r ≈ 0.5–1.9), Q2 (emerald) the second turn (r ≈ 2.2–3.7), Q3 (amber) the third turn (r ≈ 3.9–5.4), Q4 (pink) the outermost arc (r ≈ 5.7–7.1). The color coding creates clear visual sectors.

- **180 segments produce a smooth curve.** At ~6° per segment and 41.6 px/data-unit, the maximum chord error is negligible (< 0.3px). The spiral appears perfectly smooth in the rendered PNG.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All vertex counts match.** Spiral: 720/4=180 ✓. Each dot group: 2592/3=864=6×144 ✓.

- **All 22 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Parametric curves require dense tessellation.** 180 segments for 3 turns (60 per turn, ~6° each) produces a visually smooth spiral. For tighter curves, more segments per turn would be needed.

2. **Square viewports eliminate aspect correction.** With 700×700 and symmetric data ranges, px_per_dx = px_per_dy, so circles have equal X and Y radii. This is the simplest case for triAA@1 circles.

3. **Archimedean spirals map naturally to timelines.** The linearly increasing radius creates uniform spacing between turns, and the angular parameter maps to chronological order. Events placed at uniform θ intervals are evenly distributed along the arc.

4. **Origin-centered transforms (tx=ty=0) simplify polar layouts.** When the visual is symmetric around the origin, placing the data origin at clip-space (0,0) eliminates translation terms entirely.

5. **lineAA@1 at alpha 0.25 creates effective guide paths.** The low-alpha spiral line provides visual context for the dot positions without competing with the colorful event markers for attention.
