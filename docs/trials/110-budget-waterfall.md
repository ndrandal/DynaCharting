# Trial 110: Budget Waterfall

**Date:** 2026-03-22
**Goal:** Waterfall chart with 8 floating bars and dashed connector lines.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x500 viewport with one pane and two layers.

Eight floating bars showing budget flow: Revenue (+200), COGS (-70), Gross Profit (130), OpEx (-45), Marketing (-25), R&D (-25), Other Income (+15), Net Income (50). Green bars for increases, red for decreases, blue for subtotals. Dashed gray connector lines link bar tops to next bar bases.

Total: 30 unique IDs (1 pane, 2 layers, 1 transform, 9×(buf+geo+di) groups = 27)

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
- **Floating bars.** Each bar has independent y_bot and y_top, creating the waterfall cascade.
- **Color coding.** Green (gains), red (losses), blue (subtotals) follows accounting conventions.
- **Connectors.** Dashed lines at the bar-top level bridge to the next bar's base.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Each waterfall bar needs independent Y positions.** Unlike normal bars, the baseline varies per item.
2. **Use connectors to show flow continuity.** Dashed lines between bars trace the running total.
