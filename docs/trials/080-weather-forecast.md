# Trial 080: Weather Forecast

**Date:** 2026-03-22
**Goal:** Two-pane 7-day weather forecast with temperature line and precipitation bars.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

800x600 viewport with two vertically stacked panes.

1. **Temperature pane** (top) -- Orange-red lineAA trace of 7-day temperatures (16-25°C), 6 segments.
2. **Precipitation pane** (bottom) -- Blue instancedRect bars for daily precipitation (0-20mm), 7 bars.

Separate viewports for each pane with appropriate Y ranges.

Total: 9 unique IDs (2 panes, 2 layers, 2 transforms, 2 buf+geo+DI groups)

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
- **Temperature range.** Data spans 16-25°C mapped to view range [10,30] providing comfortable padding.
- **Bar widths.** Each bar spans ±0.3 data units with 0.4-unit gaps, preventing overlap.
- **Pane gap.** 0.1 clip-space separation between panes at y=−0.05 to y=0.05.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Pad data ranges.** Extending the viewport range beyond data extremes prevents clipping at edges.
2. **Match bar baseline to viewport minimum.** Precipitation bars start at y=0, viewport yMin=-2, giving a small baseline margin.
