# Trial 026: Stream Graph

**Date:** 2026-03-12
**Goal:** Five-genre music popularity stream graph across 12 months — centered stacking (symmetric above/below Y=0), creating a flowing river-like shape. First trial with triSolid@1 band triangulation between paired upper/lower curves and symmetric (non-zero-baseline) stacking arithmetic.
**Outcome:** All 55 band transitions across 5 genres are mathematically exact. Stacking is perfectly symmetric for all 12 months. Band boundaries are perfectly continuous. Genre labels centered in their bands. Zero defects.

---

## What Was Built

A 1100×600 viewport with a single pane (1012×552px, 44px left/right, 24px top/bottom margins):

**5 genre bands (triSolid@1, pos2_clip, 66 vertices each):**

Stacking order (bottom to top): Jazz, Electronic, Hip-Hop, Pop, Rock. Centered at Y=0.

| Month | Total | Base | Jazz | Electronic | Hip-Hop | Pop | Rock | Top |
|-------|-------|------|------|------------|---------|-----|------|-----|
| 0 | 100 | −50.0 | [−50,−40] | [−40,−25] | [−25,−5] | [−5,20] | [20,50] | 50.0 |
| 6 | 120 | −60.0 | [−60,−50] | [−50,−22] | [−22,13] | [13,43] | [43,60] | 60.0 |
| 11 | 108 | −54.0 | [−54,−43] | [−43,−26] | [−26,−4] | [−4,24] | [24,54] | 54.0 |

All 12 months: base = −total/2, top = total/2. Perfectly symmetric.

Each band: 11 column transitions × 6 vertices (2 triangles) = 66 vertices × 2 floats = 132 floats.

Colors (alpha 0.85): Jazz #7c3aed (purple), Electronic #3b82f6 (blue), Hip-Hop #10b981 (emerald), Pop #ec4899 (pink), Rock #f97316 (orange).

Data space: X=[−0.5, 11.5], Y=[−65, 65]. Transform: sx=0.153333, sy=0.014154, tx=−0.843333, ty=0.0 (ty=0 centers the stream at clipY=0).

Layers: Jazz (10) → Electronic (11) → Hip-Hop (12) → Pop (13) → Rock (14).

Text overlay: title, subtitle, 12 month labels, 5 genre labels positioned at month 6 center of each band.

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

- **All 55 band transitions are exact.** Verified every transition of every band (5 bands × 11 transitions). Each transition produces 6 vertices: tri1=(i,bot_i),(i,top_i),(i+1,top_{i+1}); tri2=(i+1,top_{i+1}),(i+1,bot_{i+1}),(i,bot_i). Zero errors across all 55 transitions.

- **Stacking is perfectly symmetric for all 12 months.** base = −total/2, with bands stacking upward. The top of the final band (Rock) equals total/2 = −base. Verified for all 12 months.

- **Band boundaries are perfectly continuous.** Top of Jazz = bottom of Electronic, top of Electronic = bottom of Hip-Hop, etc. Verified across all 12 months — zero gaps or overlaps between adjacent bands.

- **Transform ty=0 centers the stream.** Since the data is symmetric around Y=0, setting ty=0 maps Y=0 to clipY=0 (viewport center). This is the correct centering for a stream graph.

- **Genre labels are centered in their bands at month 6.** Jazz label at clipY=−0.778 = midpoint(−60,−50) × sy = −55 × 0.01415. Electronic at −0.510 = −36 × 0.01415. Hip-Hop at −0.064, Pop at 0.396, Rock at 0.729. All match midpoints of their month-6 band ranges.

- **The stream bulges at months 4–8.** Total popularity peaks at 120–122 (months 6–7) vs 100 (month 0). The symmetric stacking makes this visually obvious as the stream widens toward the center months.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓. All 5 geometries: 132 floats / 2 = 66 vertices, multiple of 3 (66 = 22 triangles) ✓.

- **Colors are vibrant and distinct.** Purple, blue, emerald, pink, orange create clear visual separation between adjacent bands. All hex-to-float conversions verified.

- **Background color matches spec.** #0f172a → [0.059, 0.090, 0.165] ✓.

- **All 22 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Stream graphs use centered stacking, not zero-baseline.** The key arithmetic: base = −total/2 for each column. This creates the symmetric "river" shape that distinguishes stream graphs from regular stacked area charts.

2. **triSolid@1 band triangulation between two curves is straightforward.** For N data points and 2 bounding curves (top and bottom), generate (N−1) × 2 triangles = (N−1) × 6 vertices. Each transition column needs 2 triangles to fill the quad between adjacent columns.

3. **ty=0 is the natural centering.** When data is symmetric around Y=0, the transform's ty should be 0 (or very close) so the stream centers in the viewport. The sy value then scales the symmetric range to fit the pane.

4. **Genre labels at the stream's widest point (month 6) maximize readability.** Placing labels at the fattest section of the stream ensures they fit within their band. The clipY position is computed from the band's midpoint at that month.

5. **Totals varying by month create the organic shape.** The stream widens where total popularity is highest (months 4–8 peak at 119–122) and narrows where it's lowest (month 0 at 100, month 11 at 108). This variation is the visual signature of a stream graph.
