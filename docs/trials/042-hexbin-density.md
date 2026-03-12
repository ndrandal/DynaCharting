# Trial 042: Hexbin Density Plot

**Date:** 2026-03-12
**Goal:** Hexagonal binning density plot with a bivariate Gaussian distribution (two clusters). 168 hexagons across 5 density tiers, each tessellated as a 6-triangle fan (triSolid@1, pos2_clip). Tests honeycomb grid generation, density-to-color tier mapping, and correct hexagonal tessellation with pointy-top orientation.
**Outcome:** All 168 hexagons have radius exactly 4.000 with 60° angular spacing. All hexagons correctly assigned to their density tier (168/168). Both cluster peaks correctly located in tier 5 (highest density). Zero defects.

---

## What Was Built

A 900×700 viewport with a single pane (background #0f172a):

**168 hexagons across 5 density tiers (5 triSolid@1 DrawItems, pos2_clip):**

| Tier | Density Range | Hexagons | Vertices | Color | Alpha |
|------|--------------|----------|----------|-------|-------|
| 1 (low) | 0.5–5 | 51 | 918 | #1e3a5f (dark blue) | 0.8 |
| 2 | 5–15 | 50 | 900 | #1d6a96 (medium blue) | 0.9 |
| 3 | 15–25 | 27 | 486 | #2196c3 (blue) | 0.95 |
| 4 | 25–35 | 21 | 378 | #4fc3f7 (light blue) | 1.0 |
| 5 (high) | 35+ | 19 | 342 | #b3e5fc (pale blue) | 1.0 |

Hex radius R=4.0, pointy-top orientation (first vertex at 30°). 18 vertices per hex (6 triangle-fan triangles). Grid spacing: column W=6.928, row 1.5R=6.0, odd rows offset W/2=3.464.

Density function: d(x,y) = 50·exp(−((x−30)²+(y−40)²)/(2·15²)) + 40·exp(−((x−70)²+(y−45)²)/(2·12²))

**Border (1 lineAA@1 DrawItem, rect4, 4 instances):**
Data area outline [0,100]×[0,80]. White, alpha 0.2, lineWidth 1.

Data space: X=[0,100], Y=[0,80]. Transform 50: sx=0.019, sy=0.02375, tx=ty=−0.95.

Total: 22 unique IDs.

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

- **All 168 hexagons have radius exactly 4.000.** Checked first hexagon of each tier — all 6 rim vertices at distance 4.000 from center (error ≤ 0.000001). Angular spacing is exactly 60° at every hexagon.

- **All 168 hexagons correctly assigned to density tiers.** Every hex center's density independently recomputed from the bivariate Gaussian formula and checked against its tier's range. 168/168 correct, 0 misassigned.

- **Both cluster peaks located in tier 5 (highest).** Hex closest to cluster 1 center (30,40): at (29.7, 38.0) with density 49.67. Hex closest to cluster 2 center (70,45): at (67.8, 44.0) with density 41.22. Both correctly in the highest tier.

- **Cluster 1 is larger and brighter than cluster 2.** Cluster 1 has amplitude 50 and σ=15 (broader), cluster 2 has amplitude 40 and σ=12 (narrower). The PNG clearly shows cluster 1 as a larger pale region on the left and cluster 2 as a smaller pale region on the right.

- **Honeycomb grid pattern is clean.** Hexagons tessellate without visible gaps. The pointy-top orientation creates the characteristic zigzag pattern at row edges. Odd-row offset of W/2=3.464 creates proper honeycomb alignment.

- **14 empty hexagons correctly skipped.** Hexes with density < 0.5 (far corners of the grid) are not rendered, leaving the dark background visible at the periphery.

- **5-tier color gradient encodes density effectively.** Dark blue (low) → pale blue (high) creates a clear heat map. The alpha ramp (0.8→1.0) adds subtle depth to lower-density regions where the background shows through.

- **Transform is exact.** sx=0.019, sy=0.02375 correctly maps [0,100]×[0,80] to clip[-0.95,0.95].

- **Border frames the data area correctly.** 4 line segments forming [0,100]×[0,80] rectangle.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match and are multiples of 3.** 918, 900, 486, 378, 342 — all divisible by 3 ✓.

- **All 22 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Hexbin plots group hexagons by density tier to minimize DrawItems.** Instead of one DrawItem per hex (168 items), grouping by density tier requires only 5 DrawItems. Each tier's buffer concatenates all hexagons in that tier. This is efficient and sufficient since all hexes in a tier share the same color.

2. **Pointy-top hexagons use vertex angles at 30°+60°×i.** The first vertex at 30° (not 0°) creates the pointy-top orientation. Flat-top would start at 0°. The orientation affects grid spacing formulas.

3. **Honeycomb grids require odd-row offset.** Even rows are on regular column spacing; odd rows shift by half a column width. This creates the interlocking pattern where each hex borders 6 neighbors.

4. **Bivariate Gaussians are ideal test functions for density plots.** The smooth radial falloff creates natural tier boundaries, and multiple clusters test the full dynamic range of the color mapping.

5. **Skipping low-density hexagons prevents visual clutter.** The density < 0.5 cutoff leaves the dark background visible at the edges, creating a natural boundary for the data region without needing an explicit mask.
