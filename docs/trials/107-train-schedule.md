# Trial 107: Train Schedule

**Date:** 2026-03-22
**Goal:** Time-distance diagram with 5 train paths and grid lines.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x600 viewport with one pane and two layers.

Five diagonal lines representing train journeys on a time (X, 0-24h) vs distance (Y, 0-500km) grid. Three outbound trains (A, B, C: bottom-left to top-right) and two return trains (D, E: top-left to bottom-right). Background grid has 6 horizontal lines (100km intervals) and 7 vertical lines (4h intervals). Grid on layer 10 (behind), train lines on layer 11 (in front).

Total: 21 unique IDs (1 pane, 2 layers, 1 transform, 6×(buf+geo+di)=18)

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
- **Grid-train layering.** Grid (layer 10) behind trains (layer 11) prevents grid from obscuring paths.
- **Direction encoding.** Outbound trains slope upward, return trains slope downward.
- **Color distinctness.** Each train has a unique color for identification.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use separate layers for grid and data.** Grid behind data is the standard composition pattern.
2. **Time-distance diagrams are lineAA-native.** Each train is a single line segment.
