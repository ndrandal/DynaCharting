# Trial 142: Four Waveforms

**Date:** 2026-03-22
**Goal:** 4 signal types (sine, square, sawtooth, triangle) each with 30 lineAA segments, stacked vertically in one pane.
**Outcome:** Four classic waveforms, 2 periods each, 31 sample points per waveform producing 30 lineAA@1 segments each. Zero defects.

---

## What Was Built
Viewport 900x600. Four classic waveforms, 2 periods each, 31 sample points per waveform producing 30 lineAA@1 segments each. Sine (cyan, offset +6), square (yellow, offset +2), sawtooth (green, offset -2), triangle (magenta, offset -6). Single pane with shared transform.

| DrawItem | Waveform | Pipeline | Segments | Color |
|---|---|---|---|---|
| 102 | Sine | lineAA@1 | 30 | cyan |
| 105 | Square | lineAA@1 | 30 | yellow |
| 108 | Sawtooth | lineAA@1 | 30 | green |
| 111 | Triangle | lineAA@1 | 30 | magenta |

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
- **Sine wave is smooth with continuous curvature, correctly sampled at 31 equispaced points over 2 periods.** 
- **Square wave has sharp transitions between +1 and -1 with no intermediate values.** 
- **Sawtooth wave ramps linearly from -1 to +1 then drops sharply at each period boundary.** 
- **Triangle wave is piecewise linear, ramping up then down symmetrically within each period.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Vertical offsets separate waveforms without needing multiple panes — simpler than a 4-pane layout.** 
2. **30 segments per waveform is sufficient for 2 periods: ~15 samples per period captures all features.** 
