# Trial 271: Letter Anatomy

**Date:** 2026-03-22
**Goal:** Large letter "A" built from triSolid@1 triangles with typography construction lines (baseline, cap height, crossbar) via dashed lineAA@1.
**Outcome:** Letter "A" from 9 triangles. 4 horizontal construction lines (dashed red) + 2 vertical measurement lines (dashed blue). 12 unique IDs. Zero defects.

---

## What Was Built

Viewport 500x700. Single pane with dark background.

**3 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 11 | Letter A | triSolid@1 | 9 tris | light gray |
| 105 | 10 | Construction lines | lineAA@1 | 4 segs | red, dashed |
| 108 | 10 | Measurement lines | lineAA@1 | 2 segs | blue, dashed |

Letter spans from baseline (y=-0.75) to cap height (y=0.8). Crossbar at y=-0.05.

Total: 12 unique IDs (1 pane, 2 layers, 3 buffers, 3 geometries, 3 drawItems).

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
- **Letter A is recognizable.** Two diagonal legs meeting at apex, crossbar connecting them.
- **Construction lines at key typographic positions.** Baseline, cap height, crossbar, and x-height reference.
- **Dashed lines distinguish guides from letter.** Red dashes for horizontal guides, blue for vertical measures.
- **Guides on layer behind letter.** Layer 10 (guides) behind layer 11 (letter) ensures the letter is prominent.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Manual triangulation for letterforms.** Complex shapes can be decomposed into triangle strips covering each stroke.
2. **Dashed construction lines.** dashLength + gapLength on lineAA@1 creates technical drawing aesthetics.
