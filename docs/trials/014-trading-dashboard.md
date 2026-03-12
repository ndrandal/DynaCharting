# Trial 014: Multi-Indicator Trading Dashboard

**Date:** 2026-03-12
**Goal:** Four-pane linked trading dashboard with candlesticks, SMA overlays, volume bars, RSI with reference zones, and MACD with histogram/signal. Most complex trial yet: 15 DrawItems across 4 X-axis-synchronized panes.
**Outcome:** Professional-looking trading dashboard. Price trend, SMA values, volume split, and indicator patterns all verified. Layout is pixel-perfect. Zero major defects.

---

## What Was Built

A 1200×900 viewport with 4 vertically stacked panes, all linked via `"time"` linkGroup:

**Pane 1 — Price (1176×389px, 43.2%):**
- 60 OHLC candlesticks (`instancedCandle@1`, candle6, width 0.35): green up / red down
- SMA(20) orange line (`lineAA@1`, 39 vertices from candle 20)
- SMA(50) cyan line (`lineAA@1`, 9 vertices from candle 50)
- Layer ordering: SMA below (10-11), candles on top (12)
- Price action: uptrend 100→114, consolidation ~114, selloff to 94.5, recovery to 104.3

**Pane 2 — Volume (1176×155px, 17.2%):**
- 39 green up-bars + 21 red down-bars (`instancedRect@1`, rect4). Total: 60 bars.
- Y-axis locked (panY/zoomY false).

**Pane 3 — RSI (1176×155px, 17.2%):**
- RSI(14) purple line (`lineAA@1`, 59 vertices)
- Dashed reference lines at 70 and 30 (dashLength 8, gapLength 6)
- Overbought zone y=[70, 100] faint red alpha 0.08 (`triSolid@1`)
- Oversold zone y=[0, 30] faint green alpha 0.08 (`triSolid@1`)
- Y range [0, 100], locked.

**Pane 4 — MACD (1176×161px, 17.9%):**
- MACD line blue (`lineAA@1`, 59 vertices)
- Signal line orange (`lineAA@1`, 59 vertices)
- Histogram: 32 green positive + 28 red negative bars (`instancedRect@1`)
- Dashed zero line
- Y-axis locked.

Gaps: 8px between panes. Margins: 8px top/bottom, 12px left/right.
Total: 8 + 389 + 8 + 155 + 8 + 155 + 8 + 161 + 8 = 900 ✓

Total resources: 4 panes, ~12 layers, 4 transforms, 15 buffers, 15 geometries, 15 drawItems, 4 viewports = 63 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **lineAA@1 drawItems declare rect4 format.** 8 of 15 drawItems (SMA lines, RSI line, RSI references, MACD lines, zero line) use rect4 instead of pos2_clip. Persistent issue from trials 009-012. Engine renders correctly regardless.

2. **Text labels invisible in PNG capture.** Known limitation. Without labels, the four panes have no axis annotations, no ticker symbol, no indicator names.

3. **SMA(50) line is barely visible.** Only 9 data points (candles 50-59) produce a very short line segment in the rightmost ~15% of the chart. This is mathematically correct — SMA(50) needs 50 prior data points — but visually it contributes little. Extending the dataset to 80+ candles would give the SMA(50) more coverage.

4. **Candle width 0.35 is narrow.** Standard is 0.7 for unit spacing. At 0.35, the candle bodies appear as small blocks with large gaps between them. The chart is readable but less dense than typical trading charts.

---

## Spatial Reasoning Analysis

### Done Right

- **Price trend matches specification.** Verified: uptrend 100→114 (+14), consolidation 114→114 (flat), selloff 114→94.5 (-19.5), recovery 94.5→104.3 (+9.8). All four phases visible in the candlestick pattern.

- **SMA(20) verified mathematically.** At x=20, actual value = 110.04, expected = avg(closes[1:21]) = 110.04 ✓. The SMA line tracks the price action with appropriate lag.

- **Volume split is correct.** 39 up + 21 down = 60 total ✓. Green bars correspond to candles where close ≥ open, red to close < open.

- **MACD histogram split is correct.** 32 positive + 28 negative = 60 total ✓. Histogram bars are visible with the expected pattern — positive during uptrend momentum, negative during selloff.

- **RSI zones render correctly.** The faint red (overbought) and faint green (oversold) zones are visible in the image at the correct y-ranges. Dashed reference lines at 30 and 70 are clearly visible.

- **All 4 viewports share `"time"` linkGroup.** X-axis pan/zoom is synchronized. Volume, RSI, and MACD have panY/zoomY disabled (locked Y). Price pane allows Y interaction.

- **Layout is pixel-perfect.** Four panes: 389 + 155 + 155 + 161 = 860px. Gaps: 3 × 8 = 24px. Margins: 2 × 8 = 16px. Total: 860 + 24 + 16 = 900 ✓.

- **Layer ordering correct in price pane.** SMA lines on layers 10-11 (behind), candles on layer 12 (on top). In the image, candle bodies are drawn over the SMA line — convention for trading charts ✓.

- **All 63 IDs unique.** No collisions across any resource type.

### Done Wrong

- **lineAA@1 format mismatch persists.** The agent has not corrected this across 6 trials (009-014). The rendering is unaffected, but the JSON declarations are semantically incorrect.

---

## Lessons for Future Trials

1. **Multi-pane linked dashboards work well.** The `linkGroup` mechanism correctly synchronizes X-axis pan/zoom across all 4 panes. This is the foundation for professional trading chart layouts.

2. **Indicator calculations are achievable in one shot.** SMA, RSI (Wilder's smoothing), and MACD (EMA-based) were all computed correctly in a single generation pass. The key: use standard formulas, verify at known points.

3. **Use 80+ candles for meaningful SMA(50) coverage.** With 60 candles, SMA(50) only has 10 data points. 80-100 candles would give 30-50 SMA(50) points — enough to show the trend.

4. **This is the most complex trial in the series.** 15 DrawItems, 4 panes, 63 IDs, 5 different pipeline types (instancedCandle, instancedRect, lineAA, triSolid, lineAA with dashes). It demonstrates the engine's capability for real-world trading applications.
