# Trial 075: Yin-Yang (Taijitu)

**Date:** 2026-03-12
**Goal:** Classic Yin-Yang (Taijitu) symbol using layered compositing — a full dark circle base, white right semicircle, two half-radius bulge semicircles to create the S-curve, two contrasting dots, and a circular border. Tests layer-based shape compositing, circle tessellation at multiple radii (R=40, R=20, R=5), S-curve construction via overlapping semicircles, and correct dot placement in contrasting regions, on a 600×600 square viewport.
**Outcome:** All 6 circle geometries have exact centers and radii (40.000, 20.000, 5.000). All 8 compositing test points produce correct dark/white assignment. Border circle closes perfectly (gap = 0.000000). 29 unique IDs. Zero defects.

---

## What Was Built

A 600×600 viewport (square) with a single pane (background #0f172a):

**7 DrawItems using layered compositing (6 triSolid@1, 1 lineAA@1):**

| DrawItem | Layer | Role | Pipeline | Center | Radius | Segments | Color |
|----------|-------|------|----------|--------|--------|----------|-------|
| 300 | 10 | Dark base circle | triSolid@1 | (0, 0) | 40 | 64 tris | #1e293b (dark) |
| 301 | 11 | White right semicircle | triSolid@1 | (0, 0) | 40 | 32 tris | #f8fafc (white) |
| 302 | 12 | Upper dark bulge (right half) | triSolid@1 | (0, 20) | 20 | 32 tris | #1e293b (dark) |
| 303 | 13 | Lower white bulge (left half) | triSolid@1 | (0, −20) | 20 | 32 tris | #f8fafc (white) |
| 304 | 14 | White dot (in dark region) | triSolid@1 | (0, 20) | 5 | 32 tris | #f8fafc (white) |
| 305 | 14 | Dark dot (in white region) | triSolid@1 | (0, −20) | 5 | 32 tris | #1e293b (dark) |
| 306 | 15 | Border circle outline | lineAA@1 | (0, 0) | 40 | 64 segs | #94a3b8 @ 0.8 |

All filled circles use center-fan tessellation (pos2_clip, 2 fpv). Border uses lineAA@1 (rect4, 4 fpv, lineWidth 2.0).

**S-curve compositing logic (back to front):**
1. Layer 10: Full dark circle fills entire R=40 region → all dark
2. Layer 11: Right semicircle (x ≥ 0) paints white → left dark, right white
3. Layer 12: Right half of circle at (0, 20) R=20 paints dark → pushes dark into upper-right
4. Layer 13: Left half of circle at (0, −20) R=20 paints white → pushes white into lower-left
5. Layer 14: Two dots in contrasting colors
6. Layer 15: Border outline

Data space: [−40, 40] × [−40, 40]. Transform 50: sx=sy=0.019, tx=ty=0.0. Maps to clip [−0.76, 0.76].

Total: 29 unique IDs (1 pane, 6 layers, 1 transform, 7 buffers, 7 geometries, 7 drawItems).

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

- **All 6 circle geometries have exact centers and radii.** Dark base: center (0, 0) R=40.000. Right semicircle: center (0, 0) R=40.000. Upper bulge: center (0, 20) R=20.000. Lower bulge: center (0, −20) R=20.000. White dot: center (0, 20) R=5.000. Dark dot: center (0, −20) R=5.000. All verified to 6 decimal places.

- **S-curve compositing is correct at all 8 test points.** Upper-right (15, 25) → dark (inside upper bulge). Upper-left (−15, 25) → dark (base only). Lower-right (15, −25) → white (right semicircle). Lower-left (−15, −25) → white (inside lower bulge). Center-upper-right (5, 15) → dark. Center-lower-left (−5, −15) → white. Top (0, 35) → dark. Bottom (0, −35) → white. All match the expected Yin-Yang pattern.

- **Dots are correctly placed in contrasting regions.** White dot at (0, 20) — this point is dark after compositing (inside upper dark bulge), so the white dot provides correct contrast. Dark dot at (0, −20) — this point is white after compositing (inside lower white bulge), so the dark dot provides correct contrast.

- **Border circle closes perfectly.** 64 segments trace the full 360° at R=40.000. Gap = 0.000000 between last endpoint and first startpoint. Maximum angular gap: 5.6° (uniform spacing).

- **Right semicircle correctly spans x ≥ 0.** Edge X range [0.000, 40.000] confirmed. First vertex at (0, −40), last at (0, 40), sweeping through the right half.

- **Upper bulge covers right half only (x ≥ 0).** Angle range [−90°, 90°]. Edge X range [0.000, 20.000]. This pushes dark into the upper-right area, creating the top of the S-curve.

- **Lower bulge covers left half only (x ≤ 0).** Edge X range [−20.000, 0.000]. This pushes white into the lower-left area, creating the bottom of the S-curve.

- **Layer ordering creates correct painter's algorithm compositing.** Layers 10→11→12→13→14→15, each overpainting the previous. The 6-layer approach avoids complex polygon clipping.

- **All buffer sizes match vertex counts.** 7/7 geometries verified. pos2_clip at 2 fpv for 6 fills, rect4 at 4 fpv for 1 border.

- **All 29 IDs unique.** No collisions across 1 pane, 6 layers, 1 transform, 7 buffers, 7 geometries, 7 drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Yin-Yang can be constructed via 4 overlapping circle fills + painter's algorithm.** No polygon clipping or boolean operations needed — just layer full circle (dark), right semicircle (white), upper-right bulge (dark), lower-left bulge (white). Each layer overpaints the previous.

2. **Semicircles are half of a center-fan circle.** A full circle with 64 triangles becomes a semicircle with 32 triangles by limiting the angular sweep to 180°. The center vertex stays at the circle center.

3. **S-curve boundary emerges from two half-radius semicircles.** The boundary between dark and white follows: the outer circle edge on top and bottom, transitioning through two R/2 semicircular arcs in the middle. This creates the characteristic flowing S-shape.

4. **Contrast dots must be placed at the center of the opposite-color bulge.** White dot at (0, R/2) in the dark region, dark dot at (0, −R/2) in the white region. These are the centers of the half-radius bulges.

5. **Transform sx=sy=0.019 maps [−40, 40] to [−0.76, 0.76] clip space.** This leaves ~24% margin on each side, placing the symbol comfortably within the 600×600 viewport (pixel range ~72 to ~528).
