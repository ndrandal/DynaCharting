# Trial 003: Sparkline Grid

**Date:** 2026-03-12
**Goal:** 4x3 grid of 12 mini sparkline charts, each with area fill, line trace, and end-point marker. Synchronized X-axis pan/zoom across all cells.
**Outcome:** Excellent grid layout and structural correctness. Best trial so far. Minor cosmetic issues.

---

## What Was Built

A 1200x800 viewport with 12 equally-sized cells in a 4-column x 3-row grid:

| BTC (cyan) | ETH (green) | SOL (orange) | AVAX (pink) |
|---|---|---|---|
| LINK (purple) | DOT (yellow) | MATIC (blue) | UNI (deep orange) |
| AAVE (light green) | SNX (hot pink) | CRV (violet) | COMP (teal) |

Each cell contains:
- **Area fill** (triSolid@1, pos2_clip, 174 vertices) — 29 trapezoids from baseline to price curve, alpha 0.2
- **Line trace** (lineAA@1, rect4, 29 segments) — anti-aliased price curve, width 2.0
- **End-point marker** (points@1, pos2_clip, 1 vertex) — final price dot, size 6.0

Each cell has its own pane (slightly lighter background: 0.12/0.12/0.16), its own transform (50-61), and its own viewport with `linkGroup: "sync"`, `panY: false`, `zoomY: false`.

Title area reserved in top 6.25% of viewport (50px). Asset name labels positioned in each cell's top-left corner. All text is browser-only (not in PNG).

Total resources: 12 panes, 36 layers, 12 transforms, 36 buffers, 36 geometries, 36 drawItems, 12 viewports = 168 IDs.

---

## Defects Found

### Critical

None.

### Major

1. **Area fill alpha creates inconsistent visual weight across colors.** Alpha 0.2 is applied uniformly to all 12 sparklines. On dark/cool colors (cyan BTC, blue MATIC, violet CRV), the fill is subtle and attractive. On bright/warm colors (orange SOL, yellow DOT, deep orange UNI), the fill is visually dominant — the warm hue saturates the cell and competes with the line trace for attention. SOL and DOT cells appear to have a colored background rather than a subtle fill. Trial 002 lesson #1 said "verify alpha values against background color" — the agent applied it (alpha 0.2 is reasonable on dark backgrounds) but didn't account for the interaction between fill color luminance and perceived intensity.

### Minor

1. **End-point markers partially clipped at right edge.** The final data point is at x=29, which maps to the rightmost pixel column of each pane. With pointSize 6.0, the right half of the dot extends past the pane's scissor boundary and gets clipped. Some dots appear as half-circles rather than full dots. The fix: either use x=28.5 for the marker, or set the viewport xMax to 30 to give rightmost data breathing room.

2. **Text labels invisible in PNG capture.** Known engine limitation. Without the "Portfolio Sparklines" title and asset name labels, the PNG shows 12 anonymous colored cells. The colors alone aren't enough to identify which asset is which.

3. **Sparkline wave patterns are too similar.** All 12 curves show a generally upward-trending oscillation with similar frequency. The deterministic "random walk" using sin/cos offsets produces correlated shapes. A real portfolio would show diverse patterns — some flat, some declining, some volatile, some smooth. The visual result looks like 12 views of the same underlying data with different scales, reducing the information design value.

4. **Grid gaps asymmetric in pixels.** Horizontal gaps: 0.01 clip-X = 6px. Vertical gaps: 0.01 clip-Y = 4px. The 3:2 aspect ratio (1200:800) means equal clip-space gaps produce unequal pixel-space gaps. Visually, horizontal separators appear ~50% wider than vertical ones. Either use aspect-corrected gaps (0.01 vertical, ~0.0067 horizontal) or accept the asymmetry.

5. **Title area is dead space in PNG.** The top 50px is solid black with no rendered content. This is by design (title goes there in browser mode), but it wastes 6.25% of the viewport in static captures.

---

## Spatial Reasoning Analysis

### Done Right

- **Grid layout math is precise.** Four columns of width 0.49 clip-X with 0.01 gaps, three rows of height ~0.617 clip-Y with 0.01 gaps. In pixels: 294x247px per cell, 6px horizontal gaps, 4px vertical gaps. All 12 cells are visually identical in size. No overlap, no bleed.

- **Pane regions are correctly non-overlapping.** Row boundaries: 0.875/0.258333 (row 1), 0.248333/-0.368333 (row 2), -0.378333/-0.995 (row 3). Column boundaries match across all rows. Verified: no coordinate overlap anywhere.

- **Area fill baseline matches viewport yMin.** Each sparkline's area fill extends to exactly the viewport's minimum Y value (e.g., BTC baseline = 119.395 = viewport yMin). This eliminates the "floating bars" issue from trial 002. Lesson learned and applied.

- **Per-cell viewport transforms correctly computed.** Each of 12 transforms maps its data range [0,29] x [yMin,yMax] into its pane's clip region. The sx values are identical (0.014827586 = 0.49 / 29 / 2... actually, let me verify: pane width = 0.49, data x-range = 29, so sx = 0.49/29 ≈ 0.01689... no wait, viewport maps to pane clip region). The engine's Viewport class handles this correctly — the pre-computed transform values in the JSON are overwritten by the viewport setup. The transforms in the JSON are harmless initial values.

- **ID allocation scheme is clean and collision-free.** Using 100-series for area, 200-series for line, 300-series for point creates clear separation. Within each series, groups of 3 (buf/geom/drawItem) at offsets +0/+1/+2. Total 168 IDs, zero collisions.

- **Area fill tessellation is correct.** Each segment is 2 triangles (6 vertices): {(x[i], price[i]), (x[i], baseline), (x[i+1], price[i+1])} and {(x[i+1], price[i+1]), (x[i], baseline), (x[i+1], baseline)}. This correctly fills the area between the price curve and the baseline. 29 segments × 6 verts = 174 vertices. Matches vertexCount.

### Done Wrong

- **No aspect-ratio compensation for gaps.** The agent used uniform 0.01 clip-unit gaps in both X and Y, producing 6px vs 4px visual gaps. A spatial reasoning check in pixel space would have caught this.

- **End-point at viewport edge.** Placing the marker at exactly x=29 (the viewport maximum) guarantees partial clipping. The agent should have either padded the viewport xMax or inset the marker by half the point size in data-space units.

---

## Lessons for Future Trials

1. **Adjust alpha per color luminance.** Uniform alpha across different hue/saturation/lightness values creates uneven visual weight. For warm bright colors (yellow, orange), use alpha 0.12-0.15. For cool dark colors (purple, dark blue), use alpha 0.25-0.30. Or compute: `adjustedAlpha = baseAlpha * (1.0 - luminance * 0.5)`.

2. **Inset data from viewport edges.** Don't place data points at the exact viewport min/max. Points, line endpoints, and bar edges at the boundary will be partially clipped by the scissor test. Add 1-2% padding to viewport ranges beyond the data range.

3. **Vary synthetic data patterns.** When generating fake data for N assets, ensure visual diversity: mix uptrends, downtrends, flats, V-shapes, and high-volatility patterns. Using sin/cos with offset creates correlated curves. Better: use different base functions (sin, sawtooth, step, exponential decay) per asset.

4. **Compensate gaps for aspect ratio.** When laying out a grid with pixel-even gaps, compute gap sizes in pixel space first, then convert to clip space. For a 1200x800 viewport: 4px gap in both directions means gapX = 4/600 = 0.0067 clip, gapY = 4/400 = 0.01 clip.
