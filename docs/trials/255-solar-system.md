# Trial 255: Solar System

**Date:** 2026-03-22
**Goal:** Solar system with central sun, 8 dashed orbital paths, and 8 planets (each with unique color and size) at orbital positions.
**Outcome:** Sun (16-seg), 8 orbits (32 seg each = 256 segments), 8 planets (10 tris each = 80 tris). 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Near-black space background.
Yellow sun at center (triSolid@1). 8 dashed circular orbits (lineAA@1) at increasing radii.
8 planets at various orbital positions with distinct colors: Mercury(brown), Venus(yellow), Earth(blue), Mars(red), Jupiter(orange), Saturn(gold), Uranus(cyan), Neptune(blue).
Planet sizes vary (Jupiter largest, Mercury smallest).
triGradient@1 enables per-planet coloring in one drawItem.
Total: 13 unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

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
- **Orbital radii increase logarithmically.** Inner planets closer together, outer planets more spread.
- **Planet sizes reflect relative scale.** Jupiter 0.035 vs Mercury 0.012.
- **Dashed orbits don't compete with planets visually.** Low alpha, thin lines.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **triGradient@1 for multi-colored circles avoids needing 8 separate drawItems.** Efficient for varied-color dot plots.
2. **Dashed circle outlines convey "path" rather than "boundary".**
