# Trial 008: Venn Diagram

**Date:** 2026-03-12
**Goal:** Three-circle Venn diagram using `triAA@1` with anti-aliased edges, alpha blending for overlap visibility, aspect-corrected circles, and a secondary pane with horizontal summary bars. First trial to use `triAA@1` with `pos2_alpha` for filled shapes (prior AA work was lines only).
**Outcome:** Circles are perfectly circular. AA fringe is exactly 3px. But the red and green circles fail to overlap, breaking the fundamental property of a 3-set Venn diagram.

---

## What Was Built

A 1000x750 viewport with two panes:

**Pane 1 — Venn Diagram (980×572px, 76.3%):**
- **Three AA circles** (`triAA@1`, pos2_alpha, 324 vertices each = 108 core fan + 216 fringe strip):
  - Red "Engineering" — center (-0.35, 0.15), alpha 0.35, layer 10
  - Green "Design" — center (0.35, 0.15), alpha 0.35, layer 11
  - Blue "Product" — center (0.0, -0.4), alpha 0.35, layer 12
- Each circle tessellated as triangle fan (36 segments, center + 36 edge vertices → 108 vertices) plus fringe strip (36 quads → 72 triangles → 216 vertices). Fringe vertices alternate alpha 1.0 (edge) and 0.0 (outer fringe).
- Aspect correction factor 0.5839: X radius = 0.321161, Y radius = 0.55. Data-space ellipse renders as pixel-space circle.
- Viewport: [-1.2, 1.2] × [-1.2, 1.2], full pan/zoom.

**Pane 2 — Summary Bars (980×147px, 19.6%):**
- 3 horizontal bars (`instancedRect@1`, rect4, cornerRadius 4.0):
  - Engineering: red, 72% width (660px)
  - Design: green, 65% width (593px)
  - Product: blue, 80% width (735px)
- Viewport: [-2, 102] × [-0.2, 4.0], locked (no pan/zoom).

Gap: 15px (2%) between panes. Margins: 7.9px top/bottom, 10px left/right.

Total resources: 2 panes, 6 layers, 2 transforms, 6 buffers, 6 geometries, 6 drawItems, 2 viewports = 28 IDs.

---

## Defects Found

### Critical

None.

### Major

1. **Red and green circles do not overlap.** The Engineering (red) and Design (green) circles fail to intersect, breaking the fundamental property of a 3-set Venn diagram which requires all three pairwise intersections to be non-empty.

   **Proof:** Red circle right edge at center height (y=0.15): x = -0.35 + 0.321161 = **-0.028839**. Green circle left edge at center height: x = 0.35 - 0.321161 = **+0.028839**. Gap = 0.05768 data units × 408.3 px/unit = **~24 pixels** of empty background between them. Since the maximum X extent of each circle occurs at center height, the gap exists at every Y coordinate — the circles never touch.

   **Root cause:** Circle centers are separated by 0.7 data-X units, but combined X radii = 2 × 0.321161 = 0.642 data units. Separation exceeds combined radii by 0.058 units. In pixel terms: center-to-center distance is 286px, but the circles only reach 131px from their centers, totaling 262px — falling 24px short. The centers are 2.18 radii apart; for overlap they must be < 2.0 radii apart.

   **Impact:** There is no Engineering↔Design intersection region. The text label "UX Eng" at clipY 0.36 (placed between the circles) points to empty background. The diagram has only 2 of the required 3 pairwise overlaps (red↔blue and green↔blue), and no triple overlap.

   **Fix:** Reduce horizontal separation from 0.7 to ~0.5 data units (centers at ±0.25 instead of ±0.35), or increase the base radius from 0.55 to ~0.65. Either produces all three pairwise intersections.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. The title "Team Skills Overlap", circle labels, overlap region labels ("UX Eng", "PM", "Design Ops"), bar labels, and percentages are all browser-only.

2. **Circle arrangement is not equilateral in pixel space.** Red↔Green pixel distance = 286px. Red↔Blue = Green↔Blue = sqrt(142.9² + 131.1²) ≈ 194px. The horizontal pair is 47% farther apart than the diagonal pairs. A true symmetric Venn diagram has equal center-to-center distances. This compounds the overlap failure: even if the radius were large enough, the overlaps would be highly asymmetric (thin red↔green vs. fat red↔blue / green↔blue).

3. **Blue circle drawn last has visual prominence.** Layer 12 draws on top of layers 10-11. In overlap regions, blue's alpha 0.35 composites over the already-blended red/green, making blue appear more saturated. Reversing draw order or using additive blending would distribute visual weight more evenly.

4. **Bar pane is compact.** At 147px (19.6%), the three bars + inter-bar gaps fit but leave minimal vertical breathing room. Each bar is ~28px tall. A 25% allocation (~187px) would be more proportional.

---

## Spatial Reasoning Analysis

### Done Right

- **Circles are perfectly circular.** Aspect correction factor = pane_pixel_height / pane_pixel_width = 572.25 / 980 = **0.5839**. X radius (0.321161) / Y radius (0.55) = 0.5839 ✓. Verified: pixel radius X = 0.321161 × 408.3 = 131.1px. Pixel radius Y = 0.55 × 238.4 = 131.1px. Match within 0.1px.

- **AA fringe is exactly 3 pixels in screen space.** At 0° (rightmost edge of red circle): edge vertex x = -0.028839, fringe vertex x = -0.021492. Delta = 0.007347 data units × 408.3 px/unit = 3.0px ✓. At 90° (top of red circle): edge vertex y = 0.7, fringe vertex y = 0.712582. Delta = 0.012582 data units × 238.4 px/unit = 3.0px ✓. The fringe width is correctly computed in screen space despite different X and Y scales.

- **Tessellation structure is correct.** Triangle fan: center vertex shared by all 36 triangles, each connecting two adjacent edge points. 36 × 3 = 108 core vertices. Fringe strip: each edge segment generates a quad (2 triangles, 6 vertices). 36 × 6 = 216 fringe vertices. Total: 324 per circle. Format: pos2_alpha (3 floats × 324 = 972 floats per buffer). All three circles have identical structure at different centers.

- **Alpha blending produces correct overlap colors.** With alpha 0.35 and normal blend mode, overlap regions show the expected color mixing. Red↔Blue overlap → purple/mauve, Green↔Blue overlap → teal/cyan. The blending is visible and correctly positioned where the circles geometrically intersect.

- **Layout is well-proportioned.** Pane 1: 572px (76.3%), gap: 15px (2%), pane 2: 147px (19.6%), margins: 16px (2.1%). Totals to 750px ✓. No wasted space. Directly applies lessons from trial 004 (pixel-first computation).

- **ID allocation is clean.** Groups of 3 (buffer/geometry/drawItem) at 100/101/102, 103/104/105, 106/107/108 for circles, 109/110/111, 112/113/114, 115/116/117 for bars. All 28 numeric IDs unique. Panes at 1-2, layers at 10-15, transforms at 50-51.

- **Bar proportions match stated percentages.** Engineering 72% → width 70 data units (660px), Design 65% → 63 units (593px), Product 80% → 78 units (735px). Visual bar lengths in the image match these proportions.

### Done Wrong

- **Failed to verify pairwise overlap.** The agent computed circle centers and radii independently but never checked whether all three pairs of circles actually intersect. A simple test: for circles A and B, check that `distance(center_A, center_B) < radius_A + radius_B` in pixel space. This check would have revealed that 286px > 262px for the red↔green pair.

- **Non-equilateral arrangement compounds the problem.** Placing the top two circles at y=0.15 and the bottom at y=-0.4 creates asymmetric center distances. In pixel space: horizontal = 286px, diagonal = 194px. If the agent had verified pixel distances for all three pairs, the asymmetry would have been apparent.

---

## Lessons for Future Trials

1. **Verify all pairwise overlaps for Venn/set diagrams.** After computing circle centers and radii, check `pixel_distance < sum_of_pixel_radii` for every pair. For 3 circles, that's 3 checks. For a standard Venn diagram, also verify a non-empty triple intersection by checking that the three pairwise intersection regions overlap each other.

2. **Use equilateral arrangement for symmetric Venn diagrams.** Place centers at vertices of an equilateral triangle in pixel space. Compute in pixels first: if radius = r, center separation = s where r < s < 2r (typically s ≈ 1.2r for good overlap), then the three centers form an equilateral triangle with side s. Convert to data space after.

3. **`triAA@1` with `pos2_alpha` works well for filled shapes.** The triangle fan + fringe strip technique produces clean anti-aliased edges. Key detail: fringe width must be computed in screen space (pixels), then converted to data space separately for X and Y to account for aspect ratio. This trial demonstrates the math is correct when done right.

4. **Alpha 0.35 is a good starting point for Venn overlap visibility.** With 3 overlapping circles at alpha 0.35, pairwise overlaps have effective alpha ~0.58 and triple overlap ~0.73. The progression is visible but not overwhelming. Lower alpha (0.25) would make individual circles too transparent; higher (0.5) would make overlaps nearly opaque.
