# Trial 118: Lorenz Attractor XY Projection

**Date:** 2026-03-22
**Goal:** 2D XY projection of the Lorenz attractor with 500 points. Parameters sigma=10, rho=28, beta=8/3.
**Outcome:** Lorenz attractor XY projection. 5000 Euler steps (dt=0.005) with sigma=10, rho=28, beta=8/3, subsampled to 500 points@1. Zero defects.

---

## What Was Built
Viewport 800x600. Lorenz attractor XY projection. 5000 Euler steps (dt=0.005) with sigma=10, rho=28, beta=8/3, subsampled to 500 points@1. The butterfly-wing structure of the strange attractor is visible in the (x,y) plane.

| DrawItem | Element | Pipeline | Points | Color |
|---|---|---|---|---|
| 102 | Lorenz XY | points@1 | 500 | cyan |

Total: 6 unique IDs (1 pane, 1 layer, 1 transform, 1 buffer, 1 geometry, 1 drawItem).

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
- **Lorenz system parameters (sigma=10, rho=28, beta=8/3) are the classic chaotic regime values.** 
- **Butterfly-wing double-lobe structure visible in XY projection with lobes centered near (+-sqrt(beta*(rho-1)), +-sqrt(beta*(rho-1))).** 
- **5000 integration steps with dt=0.005 provide sufficient trajectory length to outline the attractor shape.** 
- **Transform auto-fits to the data bounds with 2-unit padding on each side.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Lorenz attractor needs many integration steps (thousands) to trace out the attractor structure.** 
2. **Subsampling reduces point count for rendering while preserving overall attractor shape.** 
