# Trial 115: Bode Magnitude Plot

**Date:** 2026-03-22
**Goal:** Frequency response of a 2nd-order low-pass filter. Log-spaced X axis (20 points), magnitude in dB. lineAA@1 with dashed reference lines.
**Outcome:** Bode magnitude plot for 2nd-order low-pass (wn=100, Q=2). 21 log-spaced frequency points from 1 to 10000 rad/s produce 20 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 800x500. Bode magnitude plot for 2nd-order low-pass (wn=100, Q=2). 21 log-spaced frequency points from 1 to 10000 rad/s produce 20 lineAA@1 segments. Reference lines at 0 dB and -3 dB (dashed). Peak near resonance at ~100.0 rad/s reaching ~6.0 dB.

| DrawItem | Element | Pipeline | Segments | Color |
|---|---|---|---|---|
| 102 | Magnitude curve | lineAA@1 | 20 | cyan |
| 105 | 0 dB ref | lineAA@1 | 1 | gray |
| 108 | -3 dB ref | lineAA@1 | 1 | red |

Total: 13 unique IDs (1 pane, 2 layers, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **Magnitude follows 2nd-order transfer function |H(jw)| = 1/sqrt((1-(w/wn)^2)^2 + (w/(Q*wn))^2) correctly.** 
- **Resonance peak visible near w=wn (100 rad/s) with Q=2 giving ~6 dB gain.** 
- **High-frequency rolloff at -40 dB/decade matches 2nd-order characteristic.** 
- **Log-spaced frequencies ensure even spacing on the logarithmic x-axis.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Bode plots use log10(frequency) as X and 20*log10(magnitude) as Y.** 
2. **Log-spaced sample points are essential for even visual density across decades.** 
