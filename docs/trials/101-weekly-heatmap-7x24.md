# Trial 101: Weekly Heatmap (7x24)

**Date:** 2026-03-22
**Goal:** 7-day × 24-hour heatmap showing call volume with blue-to-red color scale.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1200x500 viewport with one pane.

168 small rectangles in a 24-column × 7-row grid. Colors represent call volume:
- Dark blue: 0-20 (very low, nighttime)
- Blue: 20-40 (low)
- Green: 40-60 (medium)
- Orange: 60-80 (high, business hours)
- Red: 80-100 (very high, peak)

Rects grouped by color bucket into 5 draw items. Small gaps between cells.

Total: 18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)

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
- **Grid layout.** 24 columns × 7 rows with 0.003 gap fills 1.7×1.7 clip space.
- **Color bucketing.** 5 color categories reduce draw items from 168 to 5.
- **Time-of-day pattern.** Seeded random data with higher base during hours 8-20 creates realistic heatmap.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Bucket continuous data for color coding.** Grouping values into 5 ranges allows batching same-color rects.
2. **Wide viewport for heatmaps.** 1200x500 gives each of 24 columns enough horizontal room.
