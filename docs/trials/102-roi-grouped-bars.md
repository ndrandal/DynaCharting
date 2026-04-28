# Trial 102: ROI Grouped Bars

**Date:** 2026-03-22
**Goal:** 4 campaigns × 3 metrics grouped bars with benchmark dashed line.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x500 viewport with one pane and two layers.

12 grouped bars in 4 campaign groups (Social, Email, Search, Display), 3 metrics each: Spend (red), Revenue (green), ROI% (blue). A white dashed benchmark line at ROI=150% overlays the bars. Shared transform maps data space to clip space.

Total: 14 unique IDs (1 pane, 2 layers, 1 transform, 3 bar groups + 1 benchmark = 4×(buf+geo+di) = 12 minus shared geo slot)

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
- **Grouped bar spacing.** 0.13 clip units between bars within a group, groups spaced 1.0 apart.
- **Benchmark overlay.** Dashed line on layer 11 renders on top of bars on layer 10.
- **Multi-scale data.** All three metrics share one Y axis (0-250), making Spend appear short relative to ROI%.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Dashed benchmark lines add context.** The 150% ROI target helps viewers judge each campaign.
2. **Consider normalized scales for mixed metrics.** Spend ($K) and ROI (%) have different magnitudes.
