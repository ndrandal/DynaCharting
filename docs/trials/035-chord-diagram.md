# Trial 035: Chord Diagram

**Date:** 2026-03-12
**Goal:** Migration flow chord diagram with 5 cities — outer ring arcs proportional to city totals, interior bowtie-shaped chords from center to inner ring sub-arcs, and straight-line connections between arc midpoints at varying alpha. Tests triSolid@1 annular arc tessellation, center-radiating chord triangles at alpha 0.3, and lineAA@1 network lines at 3 alpha levels.
**Outcome:** All 5 outer ring arcs have zero radius errors. All chord triangles pass through the center. 10 connecting lines at 3 alpha levels. 58 unique IDs. Zero defects.

---

## What Was Built

A 750×750 viewport (square) with a single pane (background #0f172a):

**5 outer ring arcs (triSolid@1, pos2_clip, R_out=6.0, R_in=5.3):**

| City | Total Flow | Arc (°) | Vertices | Color |
|------|-----------|---------|----------|-------|
| NYC | 195 | 82.6° | 102 | #3b82f6 (blue) |
| LA | 215 | 91.1° | 108 | #f97316 (orange) |
| Chicago | 140 | 59.3° | 72 | #10b981 (emerald) |
| Houston | 160 | 67.8° | 84 | #ec4899 (pink) |
| Phoenix | 140 | 59.3° | 72 | #8b5cf6 (violet) |

**5 chord group DrawItems (triSolid@1, pos2_clip, alpha 0.3):**
Bowtie shapes from center (0,0) to inner ring sub-arcs. One DrawItem per source city:
- NYC: 6 triangles (3 flows × 2 triangles each)
- LA: 8 triangles (4 flows)
- Chicago: 4 triangles (2 flows)
- Houston: 4 triangles (2 flows)
- Phoenix: 4 triangles (2 flows)

**3 alpha-grouped line DrawItems (lineAA@1, rect4):**
- High flow (≥70): 1 line, alpha 0.6 (NYC↔LA)
- Medium flow (40–69): 4 lines, alpha 0.4
- Low flow (<40): 5 lines, alpha 0.2

**5 separator lines (lineAA@1, rect4):** White, alpha 0.3, at arc boundaries.

Data space: X/Y=[−7, 7]. Transform: sx=sy=0.135714, tx=ty=0.

Text overlay: title, subtitle, 5 city labels at radius 6.8.

Total: 58 unique IDs.

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

- **All 5 outer ring arcs have zero radius errors.** Every vertex lies on either R_out=6.0 or R_in=5.3. Zero deviations across all 438 arc vertices.

- **All chord triangles pass through the center.** Each bowtie chord has one vertex at (0,0) per triangle (verified: center_hits equals triangle count for all 5 chord groups). This creates the characteristic radiating pattern of a chord diagram.

- **Arc proportions match city totals.** NYC (195/850=22.9%), LA (215/850=25.3%), Chicago (140/850=16.5%), Houston (160/850=18.8%), Phoenix (140/850=16.5%). Sum = 100%.

- **Chord alpha transparency creates visual density encoding.** At alpha 0.3, overlapping chords create darker regions where multiple flows intersect, naturally highlighting high-traffic routes.

- **Line alpha levels encode flow magnitude.** NYC↔LA (highest combined flow of 80k) gets alpha 0.6, making it the most prominent line. Low-flow pairs get alpha 0.2.

- **Square viewport eliminates aspect distortion.** Circles remain circular. Transform sx=sy ensures uniform scaling.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All 58 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Chord diagrams combine three visual elements.** Outer ring arcs (categorical identity), interior chords (flow connections), and optionally straight lines (network overlay). Each element type uses a different rendering approach.

2. **Center-radiating triangles are the simplest chord approximation.** True chord diagrams use Bézier curves, but triangles from center to ring sub-arcs create a recognizable bowtie pattern. The visual effect is similar — flows appear to connect across the diagram through its center.

3. **Grouping chords by source city enables color coding.** Each DrawItem contains all chords from one city, so the source color propagates to all its outgoing flows. Alpha 0.3 allows overlapping chords to blend.

4. **Non-sequential ID allocation works fine.** This trial used IDs starting at 300 for DrawItems (not the usual 100-series). The unified namespace only requires uniqueness, not contiguity.

5. **The ID plan used transform ID 2 and pane ID 1.** Unconventional (most trials use 50 for transforms), but valid. The engine doesn't care about ID values, only uniqueness.
