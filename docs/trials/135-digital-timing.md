# Trial 135: Digital Timing Diagram

**Date:** 2026-03-22
**Goal:** 4 digital waveforms (clock, data, enable, output) as square waves using lineAA@1, each on its own horizontal track.
**Outcome:** Digital timing diagram with 4 signals over 16 time units. Zero defects.

---

## What Was Built
Viewport 900x500. Digital timing diagram with 4 signals over 16 time units. Clock is a regular 50% duty cycle. Data is an arbitrary bit pattern. Enable gates the output. Output = Data AND Enable. Each signal uses step transitions (horizontal then vertical) for clean square waves. Signals are stacked vertically with distinct colors.

| DrawItem | Signal | Pipeline | Color |
|---|---|---|---|
| 102 | Clock | lineAA@1 | cyan |
| 105 | Data | lineAA@1 | yellow |
| 108 | Enable | lineAA@1 | green |
| 111 | Output | lineAA@1 | red |

Total: 15 unique IDs (1 pane, 1 layer, 1 transform, 4 buffers, 4 geometrys, 4 drawItems).

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
- **Clock signal toggles every time unit with perfect 50% duty cycle.** 
- **Output = Data AND Enable: output is high only when both data and enable are high.** 
- **Step transitions are rendered correctly: horizontal segment then vertical rise/fall.** 
- **Four signals are vertically separated with no overlap, each distinguishable by color.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Digital waveforms use step transitions: (t, prev_val) → (t, new_val) at each edge.** 
2. **Vertical stacking of signals in data space with a shared transform avoids multi-pane complexity.** 
