# Trial 094: Forex Candle Grid

**Date:** 2026-03-22
**Goal:** 4 currency pairs in 2x2 grid, each with 20 candlesticks.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x800 viewport with 4 panes in a 2x2 grid.

Each pane shows 20 OHLC candlesticks for a currency pair:
- **EUR/USD** (top-left)
- **GBP/USD** (top-right)
- **USD/JPY** (bottom-left)
- **AUD/USD** (bottom-right)

Green candles for bullish (close >= open), red for bearish. Each pane has its own transform auto-fitted to its price range.

Total: 20 unique IDs (4 panes, 4 layers, 4 transforms, 4×(buf+geo+di)=12)

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
- **Candle format.** candle6 (x, open, high, low, close, halfWidth) at 6 floats per candle.
- **Per-pair transforms.** Each currency has vastly different price scales (0.65 vs 148), requiring independent Y mapping.
- **Color semantics.** colorUp/colorDown auto-applied by shader based on open vs close.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use instancedCandle@1 for OHLC data.** The shader handles body/wick rendering and color selection automatically.
2. **Independent transforms for different scales.** EUR/USD (~1.08) and USD/JPY (~148) cannot share a Y axis.
