# Trial 081: IoT Sensor Grid

**Date:** 2026-03-22
**Goal:** 3x3 grid of mini sparklines showing 9 IoT sensor readings.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

900x900 viewport with 9 panes arranged in a 3x3 grid.

Each pane contains a single lineAA sparkline with 10 data points (9 segments) showing a random-walk sensor reading. Each sparkline has a unique color. Panes have subtle dark backgrounds with 0.04 clip-space margins between them.

Total: 45 unique IDs (9 panes, 9 layers, 9 transforms, 9×(buf+geo+di)=27)

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
- **Grid layout.** 3x3 pane arrangement with even spacing and consistent margins of 0.02 clip units on each side.
- **Per-pane transforms.** Each sparkline has its own transform computed from its data range, fitting data to the pane.
- **Color distinctness.** Nine different hues ensure each sensor is visually identifiable.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use per-pane transforms for independent Y ranges.** Each sensor has different magnitude so needs its own mapping.
2. **Seed random data for reproducibility.** Using random.seed(42) ensures consistent output across runs.
