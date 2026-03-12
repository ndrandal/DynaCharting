# Trial 037: Candlestick Chart

**Date:** 2026-03-12
**Goal:** 30-day OHLC candlestick chart with two linked panes — price (instancedCandle@1, candle6) and volume (instancedRect@1, rect4 split into green up-bars and red down-bars). Tests the candle6 vertex format (x, open, high, low, close, halfWidth), colorUp/colorDown per-DrawItem coloring, multi-pane layout with shared X-axis transforms, and viewport linkGroup pan/zoom synchronization.
**Outcome:** All 30 candles have valid OHLC relationships. All 29 close→next-open transitions are continuous. Volume bar up/down classification matches candle direction perfectly (21 up, 9 down). Transforms share identical sx/tx. Zero defects.

---

## What Was Built

A 1000×700 viewport with two panes (background #0f172a equivalent):

**Price pane (pane 1, clipY [−0.36, 0.95], 73% of viewport height):**

30 OHLC candles (instancedCandle@1, candle6, 1 DrawItem, 30 instances):
- Day range: 1–30, halfWidth=0.4
- Price range: low=149.00 (day 1) to high=170.50 (day 30)
- Uptrend from ~$150 to ~$170 with pullbacks at days 4, 8–9, 14–15, 20–21, 25–26
- colorUp: rgba(0.133, 0.773, 0.369, 1.0) — green
- colorDown: rgba(0.937, 0.267, 0.267, 1.0) — red

5 horizontal grid lines (lineAA@1, rect4, 5 instances):
At Y=150, 155, 160, 165, 170. White, alpha 0.06, lineWidth 1.

**Volume pane (pane 2, clipY [−0.95, −0.39], 27% of viewport height):**

21 up-volume bars (instancedRect@1, rect4, green at alpha 0.6):
Days: 1, 2, 3, 5, 6, 7, 10, 11, 12, 13, 16, 17, 18, 19, 22, 23, 24, 27, 28, 29, 30.

9 down-volume bars (instancedRect@1, rect4, red at alpha 0.6):
Days: 4, 8, 9, 14, 15, 20, 21, 25, 26.

4 horizontal grid lines (lineAA@1, rect4, 4 instances):
At Y=5, 10, 15, 20. White, alpha 0.06, lineWidth 1.

**Transforms:**
- Transform 50 (price): sx=0.0612903226, sy=0.0436666667, tx=−0.95, ty=−6.69166667
- Transform 51 (volume): sx=0.0612903226, sy=0.0224, tx=−0.95, ty=−0.95
- Shared sx/tx for linked X-axis pan/zoom.

**Viewports:**
- "price": transform 50, pane 1, linkGroup="time", X=[0,31], Y=[145,175]
- "volume": transform 51, pane 2, linkGroup="time", X=[0,31], Y=[0,25], panY=false, zoomY=false

Total: 23 unique IDs.

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

- **All 30 candles have valid OHLC relationships.** Every candle satisfies high ≥ max(open, close) and low ≤ min(open, close). All halfWidth values are exactly 0.4.

- **All 29 close→open transitions are continuous.** close[i] == open[i+1] for all adjacent candles, creating a continuous price series with no gaps.

- **Up/down classification is correct.** 21 candles have close ≥ open (green), 9 have close < open (red). The pattern matches the visible uptrend with periodic pullbacks.

- **Volume bar up/down assignment matches candle direction.** All 21 up-bar X positions match up-candle days exactly. All 9 down-bar X positions match down-candle days exactly. Zero misassignments.

- **All volume bars have correct geometry.** Every bar starts at Y=0, has width 0.8 (x±0.4 centered on candle X), and height equal to its volume value. Max volume is 21.5M (day 29), well within the Y=[0,25] range.

- **Transform math is exact.** Price pane maps [0,31]×[145,175] → clipX[-0.95,0.95]×clipY[-0.36,0.95]. Volume pane maps [0,31]×[0,25] → clipX[-0.95,0.95]×clipY[-0.95,-0.39]. All four transform components verified to full precision.

- **Linked X-axis transforms share identical sx and tx.** sx=0.0612903226 and tx=−0.95 for both transforms. Pan/zoom on the X axis will affect both panes identically via the "time" linkGroup.

- **Volume pane locks Y-axis.** panY=false, zoomY=false prevents vertical manipulation of the volume pane, which is the standard behavior for a volume sub-chart.

- **Pane regions don't overlap.** Price pane clipY [−0.36, 0.95], volume pane clipY [−0.95, −0.39]. Gap of 0.03 clip units between them creates a visible separator.

- **All clip-space positions are within pane bounds.** Spot-checked candles at days 1, 15, and 30 — all OHLC clip-Y values fall within [−0.36, 0.95]. All grid lines in both panes map to positions within their respective pane regions.

- **All vertex formats correct.** instancedCandle@1 uses candle6 ✓, lineAA@1 uses rect4 ✓, instancedRect@1 uses rect4 ✓.

- **All vertex counts match buffer data.** Candles: 180/6=30 ✓. Price grid: 20/4=5 ✓. Up bars: 84/4=21 ✓. Down bars: 36/4=9 ✓. Volume grid: 16/4=4 ✓.

- **All 23 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **instancedCandle@1 uses candle6 format (x, open, high, low, close, halfWidth).** This is the only pipeline that uses 6-float vertices. The shader handles wick/body rendering and up/down coloring automatically from colorUp/colorDown DrawItem fields.

2. **Volume bars must be split by direction for color coding.** Since instancedRect@1 uses a single `color` per DrawItem, green up-bars and red down-bars require separate DrawItems (and separate buffers/geometries). This is the standard pattern for volume sub-charts.

3. **Multi-pane linked viewports require shared sx/tx.** The "time" linkGroup means both panes scroll and zoom together on the X axis. The transforms must have identical sx and tx values. The sy and ty are independent (different Y ranges per pane).

4. **Volume pane should lock Y-axis interaction.** Setting panY=false, zoomY=false prevents the user from accidentally distorting the volume bars while pan/zooming the X axis.

5. **Close→open continuity is a strong data quality signal.** For a continuous daily series, each candle should open where the previous one closed. This is trivially satisfied by constructing the data correctly, but serves as an immediate red flag if broken.
