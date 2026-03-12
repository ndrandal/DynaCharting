# Trial 004: Bollinger Band Chart

**Date:** 2026-03-12
**Goal:** 2-pane financial chart with OHLC candles, Bollinger Bands overlay, SMA(20) line, and directional volume bars. First trial to use `instancedCandle@1`.
**Outcome:** Functionally correct — all 7 draw elements render properly. Major layout defect: 21% of viewport wasted as gap between panes.

---

## What Was Built

A 1000x700 viewport with two panes:

**Pane 1 — Price (top 45%, should be 70%):**
- 60 OHLC candlesticks (`instancedCandle@1`, candle6 format) with green (#26a69a) up / red (#ef5350) down coloring via `colorUp`/`colorDown`
- SMA(20) orange line (`lineAA@1`, 39 segments from candle 20 to 59)
- Bollinger Band fill (`triSolid@1`, 234 vertices = 39 trapezoids, blue alpha 0.12) between upper and lower bands
- Upper and lower Bollinger lines (`lineAA@1`, 39 segments each, blue, lineWidth 1.0)

**Pane 2 — Volume (bottom 32%, should be 28%):**
- 34 green up-volume bars (`instancedRect@1`, alpha 0.7) + 26 red down-volume bars (`instancedRect@1`, alpha 0.7)
- Total: 60 bars matching 60 candles

Both panes share viewport linkGroup "price" for synchronized X-axis pan/zoom. Volume pane locks Y-axis.

Right margin of 9% reserved for Y-axis labels (clipXMax = 0.82). Text overlay defines title, subtitle, and axis labels (browser-only).

Total resources: 2 panes, 5 layers, 2 transforms, 7 buffers, 7 geometries, 7 drawItems, 2 viewports = 30 IDs.

---

## Defects Found

### Critical

None. All resources created successfully, all data renders correctly, candle6 format works as expected.

### Major

1. **Massive gap between panes — 21% of viewport is dead space.** Pane 1 runs clipY [0.08, 0.98] (315px, 45%). Pane 2 runs clipY [-0.98, -0.34] (224px, 32%). The gap between them is clipY [-0.34, 0.08] = 0.42 clip units = **147px** of solid black. The spec requested 70/2/28 split (price/gap/volume). The actual split is 45/21/32. This is the most visible defect — the chart looks like two disconnected panels with a huge black bar between them rather than an integrated financial chart.

   **Root cause:** The pane regions were computed incorrectly. A 70/2/28 split of clip range [-0.98, 0.98] (1.96 total) would be: pane 1 height = 1.372, gap = 0.0392, pane 2 height = 0.5488. So pane 1: [0.5692, 0.98] wait no — it should be pane 1: clipY [-0.98 + 0.5488 + 0.0392, 0.98] = [-0.3920, 0.98]. Pane 2: [-0.98, -0.98 + 0.5488] = [-0.98, -0.4312]. The agent instead used a much smaller pane 1 and much larger gap.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Title "BTC/USDT", subtitle "Bollinger Bands (20, 2)", and all axis labels are absent.

2. **Right margin is dead space in PNG.** clipXMax = 0.82 reserves 90px on the right for Y-axis labels that are only visible in browser mode. In the PNG, this is blank black space that compresses the chart horizontally.

3. **Volume bars at alpha 0.7 appear faded.** Traditional financial charts use fully opaque volume bars. Alpha 0.7 makes the bars look washed out against the dark background. Alpha 0.85-1.0 would be more conventional.

4. **Layer ordering places SMA on top of candles.** Layer 13 (SMA) draws over layer 12 (Candles), meaning the orange line obscures candle bodies it crosses. Convention in trading charts is candles on top with indicators behind. The SMA and Bollinger layers should be below the candle layer (use lower layer IDs).

---

## Spatial Reasoning Analysis

### Done Right

- **Candle data is correctly structured.** 60 instances of candle6 (x, open, high, low, close, width) with realistic price action: uptrend (0-15), choppy (15-30), downtrend (30-45), recovery (45-60). Wicks extend correctly above/below bodies.

- **Bollinger band tessellation is correct.** 39 trapezoids (candles 20-59) with no triangle overlap. Each trapezoid uses 2 triangles: {(x[i], upper[i]), (x[i], lower[i]), (x[i+1], upper[i+1])} and {(x[i+1], upper[i+1]), (x[i], lower[i]), (x[i+1], lower[i+1])}. No double-rendering of alpha.

- **Volume bar split by direction.** Correctly partitioned 60 bars into 34 up (green) + 26 down (red) as separate DrawItems, since instancedRect only supports one color per DrawItem.

- **Viewport padding applied.** Price viewport xMin = -1.0 (1 unit before first candle at x=0) and xMax = 60.0 (1 unit after last candle at x=59). This prevents edge clipping — lesson from trial 003 applied.

- **ID allocation is clean.** Groups of 3 (buf/geom/drawItem) at 100/101/102, 103/104/105, etc. No collisions across the 30 total IDs.

### Done Wrong

- **Pane region arithmetic failed badly.** The agent produced a 45/21/32 split instead of 70/2/28. The gap alone (147px) is nearly as large as one of the panes. This suggests the agent worked in clip space without converting to pixels to sanity-check the result. A 147px black stripe between two chart panes is immediately wrong to any human viewer. This is the same spatial reasoning failure identified in trial 001: "thinking in one coordinate space at a time."

- **Right margin over-allocated.** The 90px right margin (clipXMax 0.82) occupies 9% of viewport width for axis labels that aren't rendered in PNG mode. A narrower margin (clipXMax 0.90, ~50px) would have been sufficient and given more space to the chart data.

---

## Lessons for Future Trials

1. **Always verify pane splits in pixels.** After computing clip regions, convert to pixels and state the result explicitly: "Pane 1 = X px (Y%), gap = X px (Y%), pane 2 = X px (Y%)." If any gap exceeds ~10px (~1.5%), it's probably wrong. A 2% gap at 700px tall is 14px — not 147px.

2. **Draw candles on the highest layer.** In financial charts, candles/price bars are the primary data — they should render on top of all indicators (SMA, Bollinger, grid). Put indicator layers at lower IDs than the candle layer.

3. **Don't reserve margins for text that only exists in browser mode.** If the primary output is PNG (as in trials), margins for text labels waste chart space. Either use the full pane width, or make margins minimal (~2% = 20px).

4. **Use full opacity for volume bars.** Alpha < 1.0 on volume bars against a dark background makes them look faded and unimportant. Financial chart convention: volume bars are opaque with slight color saturation difference from price candles.
