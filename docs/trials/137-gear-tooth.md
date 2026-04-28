# Trial 137: Gear Tooth Profile

**Date:** 2026-03-22
**Goal:** 8-tooth gear profile with involute tooth outlines (lineAA@1), body fill (triSolid@1), root circle, and center hole.
**Outcome:** Involute gear with 8 teeth. Zero defects.

---

## What Was Built
Viewport 600x600. Involute gear with 8 teeth. Base radius=0.35, outer radius=0.55, root radius=0.3. Gear body filled with triSolid@1 (96 vertices, 32 triangles). 8 tooth outlines (40 lineAA segments total). Root circle and center bore hole. All in clip space.

| Layer | Elements | Pipeline | Color |
|---|---|---|---|
| 10 | Body fill | triSolid@1 | dark blue |
| 11 | Tooth outlines + root circle | lineAA@1 | white/gray |
| 12 | Center hole | lineAA@1 | dim |

Total: 16 unique IDs (1 pane, 3 layers, 4 buffers, 4 geometrys, 4 drawItems).

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
- **All 8 teeth are equally spaced at 45.0 degrees around the center.** 
- **Each tooth profile has 5 segments: root-to-base, base-to-tip, across tip, tip-to-base, base-to-root.** 
- **Body fill at root radius provides the solid disc behind the tooth outlines.** 
- **Center hole indicates the shaft bore, typical of gear engineering drawings.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Gear teeth are evenly distributed: center_angle = 2*pi*i/n_teeth for tooth i.** 
2. **Tooth outline uses root_r → base_r → outer_r → outer_r → base_r → root_r profile per tooth.** 
