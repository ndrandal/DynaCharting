# Trial 009: Radar Chart

**Date:** 2026-03-12
**Goal:** Six-axis radar (spider) chart comparing two products, with concentric hexagonal grid, AA-filled data polygons, outlines, point markers, and a paired horizontal bar chart. First trial to combine `triAA@1` filled polygons with `lineAA@1` grid infrastructure.
**Outcome:** The strongest trial yet. All vertex math is exact. Hexagons are regular. AA fringe is correct. One format-declaration defect on line drawItems, but rendering is unaffected.

---

## What Was Built

A 1200×800 viewport with two panes:

**Pane 1 — Radar Chart (1176×588px, 73.5%):**
- **5 concentric hexagonal grid rings** (`lineAA@1`, 6 segments each) at 20/40/60/80/100% of max radius. Color #444 alpha 0.3, lineWidth 1.0. Each ring = 12 vertices as pos2_clip pairs forming 6 independent segments.
- **6 radial axis lines** (`lineAA@1`, 6 independent segments center→vertex) at 90°, 30°, 330°, 270°, 210°, 150°. Color #444 alpha 0.2.
- **Product A data polygon** (`triAA@1`, pos2_alpha, 54 vertices): blue #3388ff alpha 0.3. Scores: Performance 85%, Reliability 72%, Cost 65%, Features 78%, Support 82%, UX 90%. Triangle fan (18 core) + fringe strip (36 fringe).
- **Product B data polygon** (`triAA@1`, pos2_alpha, 54 vertices): orange #ff8833 alpha 0.3. Scores: Performance 68%, Reliability 88%, Cost 92%, Features 60%, Support 70%, UX 75%.
- **Polygon outlines** (`lineAA@1`, lineWidth 2.0): 12 vertices each tracing the data hexagon boundary.
- **Point markers** (`points@1`, ptSize 6.0): 6 vertices each at data polygon vertices.
- Aspect correction factor 0.5: X data radius = 0.49, Y data radius = 0.98, producing 240px pixel radius in both axes.
- Viewport [-1.2, 1.2]², static (no pan/zoom).

**Pane 2 — Score Comparison Bars (1176×180px, 22.5%):**
- 12 horizontal bars (`instancedRect@1`, rect4, cornerRadius 3.0) in 6 pairs:
  - Performance: A=85, B=68
  - Reliability: A=72, B=88
  - Cost: A=65, B=92
  - Features: A=78, B=60
  - Support: A=82, B=70
  - UX: A=90, B=75
- Blue bars (alpha 0.9) above orange bars (alpha 0.9) in each pair.
- Viewport [0, 110] × [-0.2, 12.8], static.

Gap: 16px (2%). Margins: 8px top/bottom, 12px left/right. Total: 8+588+16+180+8 = 800 ✓

Total resources: 2 panes, 10 layers, 2 transforms, 14 buffers, 14 geometries, 14 drawItems, 2 viewports = 56 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Format mismatch on lineAA@1 drawItems.** Grid rings (300-304), radial axes (305), and outlines (308-309) declare `rect4` format but use the `lineAA@1` pipeline which expects `pos2_clip`. The buffer data is structured as pos2_clip pairs (each pair = one line segment endpoint), but the geometry format says rect4 (4 floats/vertex). The engine renders correctly regardless — it appears to use the pipeline's expected attribute layout rather than the declared format — but the JSON is semantically wrong. The declared vertexCount (6) also contradicts the actual structure (12 pos2_clip vertices per hexagon ring).

2. **Bar pairs are very tightly spaced.** Within each bar pair, the gap between Product A and Product B bars is 0.1 data units = 1.4 pixels. This makes the pairs nearly indistinguishable from single fat bars. A gap of 0.3 units (4.2px) between A and B would improve visual separation while keeping the between-pair gap (currently 0.3 units = 4.2px) slightly larger for grouping hierarchy.

3. **Text labels invisible in PNG capture.** Known limitation. Without labels, the 6 axis dimensions are unidentified, the two products are distinguishable only by color, and bar scores are absent.

4. **Bar alpha 0.9 slightly faded.** Conventional practice is fully opaque bars (alpha 1.0). At 0.9, the dark background bleeds through slightly.

---

## Spatial Reasoning Analysis

### Done Right

- **Hexagons are perfectly regular.** Verified at the 100% ring: top vertex (0, 0.98) → pixel (0, 240.1), upper-right (0.424352, 0.49) → pixel (207.9, 120.1). Pixel distance from center: sqrt(0² + 240.1²) = 240.1 and sqrt(207.9² + 120.1²) = 240.1. All vertices equidistant from center ✓. Aspect correction factor 0.5 (= 588/1176) correctly compensates for the 2:1 pane aspect ratio.

- **All 12 data polygon vertices are mathematically exact.** Verified all 6 vertices of Product A and Product B against the formula `x = (score/100) × 0.49 × cos(θ)`, `y = (score/100) × 0.98 × sin(θ)`. Every vertex matches to 6 significant figures. Example: Reliability A (72%, 30°): x = 0.72 × 0.49 × 0.866 = 0.30553, vertex = 0.305534 ✓.

- **AA fringe is exactly 2.5px in screen space.** Verified at 0° (Performance axis): edge (0, 0.833) → fringe (0, 0.843204), delta = 0.010204 × 245 = 2.500px ✓. Verified at 30° (Reliability axis): delta = sqrt((0.004418×490)² + (0.005102×245)²) = sqrt(4.687 + 1.563) = 2.500px ✓. The fringe width is correctly computed per-axis in data space to produce a uniform pixel-space fringe.

- **Concentric grid rings scale exactly.** Ring fractions verified: 20% ring top = 0.196 (0.2 × 0.98 ✓), 40% = 0.392 ✓, 60% = 0.588 ✓, 80% = 0.784 ✓, 100% = 0.98 ✓. All 5 rings are correctly proportioned.

- **Layout is pixel-perfect.** Pane 1: 588px (73.5%), gap: 16px (2%), pane 2: 180px (22.5%), margins: 16px (2%). Total: 800px ✓. Directly applies trial 004's lesson.

- **Layer ordering is correct.** Grid (10) → radials (11) → fills (12-13) → outlines (14-15) → points (16-17). Data is rendered over grid, outlines over fills, points on top. Draw order matches visual hierarchy.

- **Bar widths match scores exactly.** Product A bars: 85, 72, 65, 78, 82, 90. Product B bars: 68, 88, 92, 60, 70, 75. All widths verified against the rect4 data (x2 - x1 = score). Bars are correctly paired by dimension with A consistently above B.

- **All 56 IDs are unique.** Panes 1-2, layers 10-17/20-21, transforms 50-51, buffers 100-113, geometries 200-213, drawItems 300-313. Century-range pattern (buffers 100s, geometries 200s, drawItems 300s) prevents collisions across resource types.

### Done Wrong

- **Used wrong format for lineAA drawItems.** The agent declared `rect4` format for 8 of 14 geometries, all paired with `lineAA@1`. The correct format is `pos2_clip`. This likely happened because the agent used a single format variable when generating geometry declarations. The data itself IS structured as pos2_clip pairs, so the visual output is correct — but a stricter engine validation mode would reject these drawItems.

---

## Lessons for Future Trials

1. **Match vertex format to pipeline.** `lineAA@1` requires `pos2_clip`, `triAA@1` requires `pos2_alpha`, `points@1` requires `pos2_clip`. Always verify format declarations match the pipeline. The engine may tolerate mismatches but the JSON should be self-consistent.

2. **Radar chart polar-to-cartesian math is straightforward.** For N axes at angles θ₀..θₙ₋₁ with aspect correction factor A: `x = (score/max) × R × A × cos(θ)`, `y = (score/max) × R × sin(θ)`. Verify regularity by computing pixel distance from center for each vertex — all should be equal for the grid rings.

3. **For paired bar charts, ensure visible gaps.** A minimum gap of ~3-4px between bars within a pair is needed for visual distinction. With 12 bars in 180px vertical space, that's ~15px per bar + gap. Budget carefully.

4. **This is the first trial with zero major defects since trial 005.** The combination of pixel-first layout, mathematical vertex computation, and systematic ID allocation produced a clean result. The only issues are declaration-level (format mismatch) and cosmetic (bar spacing, alpha).
