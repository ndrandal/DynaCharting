# Trial 041: Contour Map

**Date:** 2026-03-12
**Goal:** Filled contour/topographic map with 8 elevation bands around a Gaussian peak at (55, 50). Peak is slightly off-center. Tests back-to-front filled-circle layering to create annular contour bands (triSolid@1 triangle fans), contour line overlay (lineAA@1, 504 segments across 7 contours), and terrain-appropriate color gradient (green lowlands → brown summit).
**Outcome:** All 7 filled circle radii match the Gaussian contour formula r = σ√(−2 ln(L/100)) to zero error. All circles centered at (55, 50). Background rectangle covers [0,100]×[0,100]. All 7 contour lines at exact radii. Zero defects.

---

## What Was Built

An 800×800 viewport (square) with a single pane (background #0f172a):

**8 filled elevation bands (8 triSolid@1 DrawItems, pos2_clip):**

| Band | Layer | Elevation Range | Shape | Radius | Color |
|------|-------|----------------|-------|--------|-------|
| 0 | 10 | 0–12.5 | Rectangle [0,100]² | — | #1a4731 (deep green) |
| 1 | 11 | 12.5–25 | Filled circle | 44.865 | #1e6b3a (forest green) |
| 2 | 12 | 25–37.5 | Filled circle | 36.632 | #2d8f4e (green) |
| 3 | 13 | 37.5–50 | Filled circle | 30.813 | #6aaa5c (sage) |
| 4 | 14 | 50–62.5 | Filled circle | 25.903 | #a5c070 (yellow-green) |
| 5 | 15 | 62.5–75 | Filled circle | 21.330 | #d4b86a (tan) |
| 6 | 16 | 75–87.5 | Filled circle | 16.688 | #c4855c (brown) |
| 7 | 17 | 87.5–100 | Filled circle | 11.369 | #a85a48 (dark brown) |

Band 0: 2 triangles (6 vertices) covering full data rectangle. Bands 1–7: 36-sector triangle fans (108 vertices each = 36 triangles from center). Back-to-front layering creates annular appearance.

**7 contour lines (1 lineAA@1 DrawItem, rect4, 504 instances = 7 × 72):**
White, alpha 0.2, lineWidth 1. One 72-segment circle per contour level on layer 18 (top).

Gaussian elevation function: z(x,y) = 100 × exp(−((x−55)² + (y−50)²) / (2×22²)). Peak at (55, 50), σ=22.

Data space: X=Y=[0, 100]. Transform 50: sx=sy=0.019, tx=ty=−0.95.

Total: 38 unique IDs.

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

- **All 7 filled circle radii match the Gaussian formula to zero error.** Each contour radius r = 22 × √(−2 ln(L/100)) independently computed and compared to buffer data. All 7 circles have zero radius error across all 36 rim vertices.

- **All 7 circles centered at (55, 50).** The peak is correctly positioned slightly right of center (55 vs center 50 on X axis). All circle fans share this center.

- **Back-to-front layering creates clean annular bands.** Each filled circle on a higher layer occludes the previous, creating the appearance of annular rings without complex polygon subtraction. Layer 10 (background) → 11 → 12 → ... → 17 (summit) → 18 (contour lines on top).

- **Background rectangle covers the full data space.** Two triangles spanning [0,100]×[0,100] ensure no background gaps outside the largest contour circle.

- **All 7 contour lines at exact radii.** 504 line segments (72 per contour) verified — first and second endpoint of each contour's first segment both match the expected radius to zero error.

- **Color gradient is terrain-appropriate.** Green tones for lower elevations (0–50%), transitioning through yellow-green and tan to brown for highest elevations. The visual reads naturally as a topographic map.

- **Off-center peak is visible.** The bullseye pattern is shifted 5 data units rightward (center at x=55 vs data center x=50), creating an asymmetric gap between the circles and the left/right rectangle edges. This is clearly visible in the PNG.

- **36 sectors provide smooth circular appearance.** At 10° per sector, the polygonal approximation is imperceptible at 800×800 resolution (maximum chord error < 0.4 data units).

- **Contour lines at alpha 0.2 provide subtle boundary definition.** The white contour lines are visible enough to delineate band boundaries without dominating the color-coded elevation bands.

- **Square viewport ensures circular contours.** 800×800 with symmetric transform (sx=sy) means circles remain perfectly circular.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match.** Rectangle: 12/2=6 ✓. Each circle: 216/2=108 ✓. Contour lines: 2016/4=504 ✓.

- **All 38 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Back-to-front filled circles simulate contour bands without polygon subtraction.** Instead of computing annular ring geometry (complex), simply layer filled disks from largest (back) to smallest (front). Each disk occludes the previous, naturally creating the annular appearance. This is the key insight for contour map rendering with simple primitives.

2. **Gaussian contour radii have a clean closed form.** For z = A·exp(−r²/2σ²), the contour at level L has r = σ·√(−2·ln(L/A)). This makes computing exact contour positions trivial.

3. **A background rectangle is needed for the outermost band.** The area outside the largest contour circle but inside the data range must be filled. A simple full-coverage rectangle on the lowest layer handles this.

4. **Terrain color conventions aid readability.** Green → yellow → brown is the standard topographic color scheme. Users immediately recognize the pattern as elevation data without needing a legend.

5. **Off-center features test asymmetric rendering.** Placing the peak at (55, 50) instead of (50, 50) verifies that the engine correctly handles non-centered data. The asymmetric gap to the left/right edges is a visual confirmation.
