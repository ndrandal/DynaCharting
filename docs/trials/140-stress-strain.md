# Trial 140: Stress-Strain Curve

**Date:** 2026-03-22
**Goal:** Material stress-strain curve with elastic, yield, strain hardening, and necking regions. 40 points total. Yield point marked.
**Outcome:** Stress-strain curve for mild steel. 4 regions: elastic (linear, E=200), yield plateau (sigma_y=250), strain hardening (to UTS=400 at strain=15), necking (to fracture at strain=22, sigma=300). 40 total data points producing 39 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 800x500. Stress-strain curve for mild steel. 4 regions: elastic (linear, E=200), yield plateau (sigma_y=250), strain hardening (to UTS=400 at strain=15), necking (to fracture at strain=22, sigma=300). 40 total data points producing 39 lineAA@1 segments. Red yield point marker and dashed yield stress reference.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | Stress-strain | lineAA@1 | 39 seg | cyan |
| 105 | 12 | Yield point | points@1 | 1 pt | red |
| 108 | 10 | Yield ref | lineAA@1 | 1 seg | dim dashed |

Total: 14 unique IDs (1 pane, 3 layers, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **Elastic region is perfectly linear with slope E=200 (Young's modulus).** 
- **Yield plateau at 250 shows the characteristic flat region of mild steel.** 
- **Strain hardening shows a concave-up rise from yield to ultimate tensile strength.** 
- **Necking region shows stress decrease after UTS, ending at fracture — correct for ductile materials.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Stress-strain curves have 4 distinct regions, each with different mathematical behavior.** 
2. **Yield point marker (red dot) highlights the transition from elastic to plastic deformation.** 
