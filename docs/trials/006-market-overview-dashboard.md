# Trial 006: Market Overview Dashboard

**Date:** 2026-03-12
**Goal:** Asymmetric 4-pane dashboard combining candlestick/volume charts with a donut chart and horizontal bar chart. First trial with arc tessellation and asymmetric grid layout.
**Outcome:** Most ambitious trial yet — all 4 panes render correctly. Donut is circular. Layout is clean with no wasted space.

---

## What Was Built

A 1200x800 viewport with an asymmetric 2-column, 4-pane layout:

**Left column (67%, 772px wide):**
- **Pane A "Price"** (515px tall) — 80 OHLC candlesticks (`instancedCandle@1`, candle6) with green/red coloring, orange SMA(20) overlay (`lineAA@1`, 59 segments). Price action shows uptrend → consolidation → selloff → bounce.
- **Pane B "Volume"** (238px tall) — 80 volume bars split into 50 green up-bars + 30 red down-bars (`instancedRect@1`). Spikes at trend reversals.
- Linked via `"time"` linkGroup for synchronized X-axis pan/zoom.

**Right column (33%, 380px wide):**
- **Pane C "Allocation"** (415px tall) — 5-sector donut chart (`triSolid@1`) representing crypto portfolio: BTC 38% (orange), ETH 24% (purple-blue), SOL 15% (green), ADA 13% (dark blue), Other 10% (gray). Each sector tessellated as 20 trapezoids (40 triangles, 120 vertices). Aspect ratio corrected for circular appearance.
- **Pane D "Rankings"** (338px tall) — 5 horizontal bars (`instancedRect@1` with cornerRadius 3.0) matching donut sectors, proportional to percentages.

Gaps: 16px vertical between panes, 24px between columns, 12px outer padding. Background colors: left panes blue-dark, right panes purple-dark.

Total resources: 4 panes, 14 layers, 4 transforms, 14 buffers, 14 geometries, 14 drawItems, 4 viewports = 64 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the donut sectors are identified only by color, the horizontal bars have no names/percentages, and the candle chart has no ticker or axis labels.

2. **Donut edges are jagged (no anti-aliasing).** The donut uses `triSolid@1` which has no per-vertex alpha for edge smoothing. At 20 segments per sector, the arc approximation is smooth but the outer and inner edges show staircase aliasing. Using `triAA@1` with `pos2_alpha` format would produce smoother edges, but at significantly higher vertex complexity. Acceptable tradeoff for a first donut implementation.

3. **Right column is 33% width instead of spec's 31%.** The spec's 67%+31% = 98% left 23px of rounding slack, which the agent allocated to the right column (357+23 = 380px). This is a reasonable choice — the extra width benefits the donut and bars — but doesn't match the spec exactly.

4. **Volume bar alpha (0.85) slightly faded.** Improvement over trial 004's 0.7, but still not fully opaque. Traditional charts use alpha 1.0 for volume bars.

5. **Donut sector gaps may be inconsistent.** The spec requested 1% angular gaps between sectors. Some gaps are visible in the image between larger sectors (BTC/ETH) but less visible between smaller sectors (ADA/Other) where the gap is proportionally the same angle but fewer pixels wide.

---

## Spatial Reasoning Analysis

### Done Right

- **Asymmetric layout is pixel-perfect.** Every pane dimension matches the spec's pixel targets:
  - Pane A: 772×515px (67%×67%) ✓
  - Pane B: 772×238px (67%×31%) ✓
  - Pane C: 380×415px (33%×54%) ✓
  - Pane D: 380×338px (33%×44%) ✓
  - Gaps: 16px vertical, 24px horizontal ✓
  - No wasted space — directly addresses trial 004's 21% dead gap.

- **Donut is circular in pixel space.** The BTC sector (spanning 0°–137°, wide enough to reach both max-X and max-Y) has pixel radius 176.3px in X and 176.4px in Y — a match within 0.1 pixel. The aspect correction factor (1.092) correctly compensates for pane C being 380px wide × 415px tall. The donut is an ellipse in data space (X stretched by 1.092×) that renders as a circle in pixels.

- **Donut tessellation is structurally correct.** 5 sectors × 120 vertices × 2 floats = 240 floats per buffer. Each sector has 20 trapezoid segments between inner and outer arcs, tessellated as 40 triangles. Vertex counts match buffer sizes.

- **Candle + volume pipeline reused correctly.** Same pattern as trial 004 but with correct layout this time. 80 candles, 59 SMA segments (from candle 20), 80 volume bars split 50/30 by direction.

- **ID allocation uses clean separation.** Buffers 100-113, geometries 200-213, drawItems 300-313 — each resource type in its own century range. 64 total IDs, zero collisions.

- **Viewport design is appropriate per pane.** Left panes use linked viewports with data-space ranges. Donut pane uses a square data range [-1,1]×[-1,1] with identity-like transform (viewport handles mapping). Bar pane uses a simple [0,40]×[0,6] range for horizontal layout.

### Done Wrong

- **No aspect-ratio documentation in the output.** The donut aspect correction is the most complex spatial reasoning in any trial, but the JSON doesn't contain comments explaining the correction factor. Future maintainers (or AI agents reading this trial) won't understand why the donut vertices are elliptical in data space without external documentation.

---

## Lessons for Future Trials

1. **Donut charts are achievable with `triSolid@1`.** Arc tessellation with 20 segments per sector produces smooth curves. The key is aspect ratio correction: multiply X coordinates by `(pane_pixel_height / pane_pixel_width)` to make data-space ellipses render as pixel-space circles. Verify by checking that the max pixel extent in X equals the max pixel extent in Y for a sector spanning at least 90°.

2. **Asymmetric layouts require pixel-first planning.** Computing pane regions in clip space directly leads to errors (trial 004). The correct workflow: define pixel dimensions → convert to clip space → verify round-trip. This trial's pixel-perfect layout proves the approach works.

3. **The ID century-range pattern scales well.** Buffers in 100s, geometries in 200s, drawItems in 300s avoids collisions even with 14 draw elements. This is cleaner than the groups-of-3 pattern used in earlier trials.

4. **For smoother donut edges, use `triAA@1`.** The `pos2_alpha` format (3 floats: x, y, alpha) allows per-vertex alpha for anti-aliased edges. Each arc segment would need additional fringe vertices at alpha=0 outside the outer edge and inside the inner edge. This roughly triples vertex count but eliminates staircase aliasing. Worth attempting in a future trial.

5. **Mixed interactive + static panes work well.** Left panes with linked viewports for pan/zoom coexist cleanly with right panes that have non-interactive viewports. No linkGroup on the donut/bar viewports means they're unaffected by scrolling the candle chart.
