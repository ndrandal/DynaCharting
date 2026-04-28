# Trial 133: Electromagnetic Spectrum

**Date:** 2026-03-22
**Goal:** Horizontal bar showing 7 electromagnetic spectrum bands (Radio to Gamma) as instancedRect@1 colored rectangles.
**Outcome:** Electromagnetic spectrum visualization with 7 instancedRect@1 bands arranged horizontally. Zero defects.

---

## What Was Built
Viewport 900x300. Electromagnetic spectrum visualization with 7 instancedRect@1 bands arranged horizontally. From left to right: Radio (gray), Microwave (brown), Infrared (red), Visible (yellow), Ultraviolet (purple), X-ray (blue), Gamma (green). Each band occupies an equal width, centered vertically.

| DrawItem | Element | Pipeline | Color |
|---|---|---|---|
| 102 | Radio | instancedRect@1 | gray |
| 105 | Microwave | instancedRect@1 | brown |
| 108 | Infrared | instancedRect@1 | red |
| 111 | Visible | instancedRect@1 | yellow |
| 114 | Ultraviolet | instancedRect@1 | purple |
| 117 | X-ray | instancedRect@1 | blue |
| 120 | Gamma | instancedRect@1 | green |

Total: 23 unique IDs (1 pane, 1 layer, 7 buffers, 7 geometrys, 7 drawItems).

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
- **Seven bands are arranged left-to-right in order of increasing frequency (Radio → Gamma).** 
- **Each band has a distinct color representing its approximate wavelength or convention.** 
- **Small gaps between bands create visual separation without a visible grid.** 
- **Wide viewport (900x300) creates a natural horizontal bar layout.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **EM spectrum visualizations use equal-width bands for conceptual frequency ranges.** 
2. **instancedRect@1 is ideal for simple rectangular color blocks with no transform needed.** 
