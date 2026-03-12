# Trial 021: Slope Chart

**Date:** 2026-03-12
**Goal:** Eight-language slope chart comparing programming language popularity rankings between 2019 and 2024 — sloped lines connecting left/right rank columns, color-coded by direction (green risers, red fallers), with circular endpoint markers. First trial with crossing lines and inverted Y-axis (rank 1 at top).
**Outcome:** Mathematically exact in every dimension. All 8 line endpoints, 16 circle centers, aspect correction, fringe widths, and label positions verified to sub-pixel precision. Zero defects.

---

## What Was Built

An 800×700 viewport with a single pane (460×595px, 170px left/right margins, 52.5px top/bottom):

**8 connecting lines across 2 lineAA@1 DrawItems (rect4 format):**

| Language | 2019 Rank | 2024 Rank | Direction | Color |
|----------|-----------|-----------|-----------|-------|
| Python | 3 | 1 | Rose | Green (#36d399) |
| TypeScript | 7 | 3 | Rose | Green |
| Rust | 8 | 6 | Rose | Green |
| JavaScript | 1 | 2 | Fell | Red (#f87171) |
| Java | 2 | 4 | Fell | Red |
| C++ | 4 | 5 | Fell | Red |
| C# | 5 | 8 | Fell | Red |
| Go | 6 | 7 | Fell | Red |

Green DrawItem: 3 instances, lineWidth 2.5, alpha 0.9. Red DrawItem: 5 instances, lineWidth 2.0, alpha 0.6.

**16 endpoint circles across 2 triAA@1 DrawItems (pos2_alpha format):**

Green circles (6): Python left (0,3), Python right (1,1), TypeScript left (0,7), TypeScript right (1,3), Rust left (0,8), Rust right (1,6).

Red circles (10): JavaScript left (0,1), JavaScript right (1,2), Java left (0,2), Java right (1,4), C++ left (0,4), C++ right (1,5), C# left (0,5), C# right (1,8), Go left (0,6), Go right (1,7).

Circle radii: X=0.017391, Y=0.107563 data units. Pixel radius: 8.000px in both axes. Aspect correction factor: 0.16168 (px_per_dy/px_per_dx = 74.375/460).

16 angular segments per circle. Core: 48 vertices (alpha=1). Fringe: 96 vertices (alpha 1→0). Total: 144 per circle.

Data space: X=[0, 1], Y=[0.5, 8.5]. Transform: sx=1.15, sy=−0.2125, tx=−0.575, ty=0.95625. Inverted Y maps rank 1 to top (clipY=0.74375), rank 8 to bottom (clipY=−0.74375).

Layers: lines (10, behind), circles (11, on top).

Text overlay: title, subtitle, "2019"/"2024" column headers, 16 rank labels (8 left right-aligned, 8 right left-aligned), color-coded green/red.

Total: 1 pane, 2 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems = 16 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the languages and rank numbers are absent — the green/red color coding and line slopes are the only cues.

---

## Spatial Reasoning Analysis

### Done Right

- **All 8 line segments connect correct rank positions.** Python (0,3)→(1,1), TypeScript (0,7)→(1,3), Rust (0,8)→(1,6), JavaScript (0,1)→(1,2), Java (0,2)→(1,4), C++ (0,4)→(1,5), C# (0,5)→(1,8), Go (0,6)→(1,7). All verified against the data table.

- **All 16 circle centers are exact.** Each circle is centered on its language's rank position at X=0 (2019) or X=1 (2024). All 16 verified to machine precision.

- **All circles are perfectly circular.** X data radius (0.017391) × px_per_dx (460) = Y data radius (0.107563) × px_per_dy (74.375) = 8.000px. The aspect correction ratio of 0.16168 is correctly applied — X radius is ~6.2× smaller than Y radius in data space, compensating for the ~6.2× wider X pixel scale.

- **Fringe is exactly 2.5px in both axes.** X fringe: 0.005435 × 460 = 2.500px. Y fringe: 0.033613 × 74.375 = 2.500px. Aspect-corrected independently.

- **All 16 core perimeter vertices are exact.** Verified every angular segment of the first green circle against expected cos/sin values at 16 equally-spaced angles. All match to <1e-4 data units.

- **Inverted Y-axis works correctly.** Negative sy (−0.2125) maps rank 1 → clipY 0.74375 (top) and rank 8 → clipY −0.74375 (bottom). The transform is mathematically exact.

- **Color coding is correct.** Green (#36d399 → [0.212, 0.827, 0.6]) for risers (Python, TypeScript, Rust), red (#f87171 → [0.973, 0.443, 0.443]) for fallers (JavaScript, Java, C++, C#, Go). Hex-to-float conversions verified.

- **Line styling differentiates risers from fallers.** Green lines are thicker (2.5 vs 2.0) and more opaque (0.9 vs 0.6), making the "winners" visually prominent. This is a good design choice.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓. Zero format mismatches.

- **All vertex counts match buffer sizes.** Green lines: 12 floats / 4 = 3 instances ✓. Red lines: 20 / 4 = 5 ✓. Green circles: 2592 / 3 = 864 vertices = 6 × 144 ✓. Red circles: 4320 / 3 = 1440 = 10 × 144 ✓.

- **Layer ordering correct.** Lines on layer 10 (behind), circles on layer 11 (on top). Circles render over line endpoints.

- **Text label positions match transform output.** All 16 rank labels positioned at clipY = rank × (−0.2125) + 0.95625, matching the transform exactly. Left labels right-aligned at clipX=−0.605, right labels left-aligned at clipX=0.605.

- **All 16 IDs unique.** Systematic triplet allocation (100–111) plus structural IDs (1, 10, 11, 50). No collisions.

- **Background color correct.** Clear color [0.102, 0.102, 0.18] = #1a1a2e ✓.

- **Crossing lines create the characteristic slope chart pattern.** TypeScript's dramatic rise (7→3) crosses multiple falling lines, Python's rise (3→1) crosses JavaScript and Java, and Rust's rise (8→6) crosses Go. The visual immediately communicates rank changes.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Slope charts combine lineAA@1 and triAA@1 effectively with shared transforms.** Two color groups (green/red) need only 2 line DrawItems and 2 circle DrawItems. All share one transform for consistent positioning.

2. **Inverted Y-axis via negative sy works cleanly.** For rank-based charts where rank 1 should be at the top, use negative sy in the transform. No special handling needed — the engine treats negative scales correctly.

3. **Non-square aspect correction with extreme ratios is reliable.** This chart had px_per_dx = 460 vs px_per_dy = 74.375 (ratio 6.18:1), the second most extreme after Trial 020 (6.926:1). The circle tessellation handles it perfectly.

4. **Grouping by color is efficient for slope charts.** Rather than one DrawItem per language (8 line + 8 circle = 16 DrawItems), grouping by color (2 line + 2 circle = 4 DrawItems) reduces resource count while maintaining visual clarity. Each group shares a single buffer with all instances concatenated.

5. **Line thickness and alpha can encode direction.** Making risers thicker and more opaque than fallers creates a natural visual hierarchy without requiring the viewer to parse colors alone. This dual-encoding (color + weight) improves readability.
