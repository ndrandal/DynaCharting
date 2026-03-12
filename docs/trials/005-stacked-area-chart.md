# Trial 005: Stacked Area Chart

**Date:** 2026-03-12
**Goal:** 4-series stacked area chart showing market share over 24 months, with boundary lines and proper stacking math. No alpha doubling.
**Outcome:** Cleanest trial yet. Structurally flawless, visually strong, correct stacking math.

---

## What Was Built

A 1100x650 viewport with a single pane displaying four stacked area bands:

- **Blue (Desktop)** — bottom band, declining from ~56% to ~35%
- **Green (Mobile)** — second band, growing from ~25% to ~42%
- **Orange (Tablet)** — third band, stable ~12-16% with mid-period peak
- **Purple (Other)** — top band, thin ~5-10% strip filling to ~100%

Each band has:
- A `triSolid@1` area fill (138 vertices = 23 trapezoids) tessellated between its own bottom and top boundaries
- A `lineAA@1` boundary line (23 segments) on top of the fill for crisp edges

Layer ordering: area fills on layers 10-13 (drawn first), boundary lines on layers 14-17 (drawn on top).

Single viewport with X pan/zoom enabled, Y locked. Viewport range [-0.5, 23.5] x [-2, 105] provides padding around the [0, 23] x [0, 100] data range.

Text overlay: centered title, Y-axis percentage labels (0-100%), X-axis month labels (Jan-Oct), and a 4-item color legend in the top-right corner.

Total resources: 1 pane, 8 layers, 1 transform, 8 buffers, 8 geometries, 8 drawItems, 1 viewport = 34 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the four color bands are distinguishable by color but there's no way to identify which is which, read percentages, or see time axis. The legend, title, and axis labels all exist in the text overlay but only render in browser mode.

2. **Alpha 0.75 is unnecessary for non-overlapping fills.** Each area band is correctly tessellated between its own boundaries (B's fill starts at top-of-A, not at y=0). Since the fills don't overlap, there's no risk of alpha doubling. Alpha 1.0 would produce more vibrant, saturated colors. At 0.75, the dark background bleeds through, slightly muting each band. This is subtle — the colors still look good — but opaque fills would be more conventional for stacked area charts.

3. **Boundary lines have low contrast against fills.** The line colors are slightly brighter versions of the fill colors (e.g., fill #1565C0 vs line #1976D2). On top of the 0.75-alpha fill, the contrast between line and fill is subtle. The lines serve their purpose (crisp band edges) but are not prominent visual elements. Using white or near-white thin lines (alpha 0.5) between bands would create stronger visual separation.

4. **"Other" band is very thin in places.** Around months 8-9, the purple strip narrows to ~5% of the y-range, which at 598px pane height is ~30px. Still visible, but the boundary lines above and below are close together. This is accurate to the data — not a bug — but the thin strip reduces readability.

---

## Spatial Reasoning Analysis

### Done Right

- **Pane layout is correct and well-proportioned.** Single pane at clipY/X [-0.92, 0.92] = 598x1012px with 26-44px margins. No wasted space, no giant gaps. This directly addresses trial 004's major defect (21% dead gap). The agent got the layout right on the first try.

- **Stacking math is correct.** Each band's bottom boundary equals the previous band's top boundary. Desktop bottom = 0, Mobile bottom = Desktop top, Tablet bottom = Mobile top, Other bottom = Tablet top. The cumulative sums reach ~100% at each time point. No gaps or overlaps between bands in the visual output.

- **No alpha doubling.** Critical requirement met. Each triSolid fill covers only its own vertical range, not overlapping with other fills. The visual confirms: colors are uniform within each band, no darker regions from accumulation.

- **Viewport padding applied.** xMin=-0.5 (half unit before month 0), xMax=23.5 (half unit after month 23), yMin=-2 (2% below zero), yMax=105 (5% above 100%). Data doesn't touch the pane edges. Lesson from trial 003 applied.

- **Layer ordering correct.** Fills drawn first (layers 10-13), lines drawn on top (layers 14-17). Lines are visible and crisp over the fill areas. Drawing order within each group follows the stacking order (Desktop first, then Mobile on top, etc.).

- **ID allocation clean.** Groups of 3 (buf/geom/drawItem) at 100-123 for fills, 112-123 for lines. All 34 IDs unique with no collisions.

### Done Wrong

- Nothing structurally wrong. The only issues are cosmetic choices (alpha, line contrast) that could be improved but aren't errors.

---

## Lessons for Future Trials

1. **Use opaque fills for non-overlapping stacked areas.** Alpha < 1.0 only matters when fills overlap. For proper stacked areas where each band occupies its own vertical slice, alpha 1.0 produces cleaner results. Reserve semi-transparency for overlapping fills (like Bollinger bands in trial 004).

2. **Use contrasting line colors between bands.** White or light gray boundary lines (alpha 0.3-0.5) create clearer band separation than slightly-brighter versions of the fill color. Alternatively, omit boundary lines entirely if the fill colors have enough contrast between adjacent bands.

3. **This trial's pane layout should be the baseline.** Single pane at [-0.92, 0.92] with symmetric margins is clean and simple. Multi-pane layouts should follow the same discipline: compute sizes in pixels, verify proportions match the spec.
