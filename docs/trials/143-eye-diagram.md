# Trial 143: Eye Diagram

**Date:** 2026-03-22
**Goal:** 20 overlaid bit-period traces (lineAA@1) creating an eye pattern with low alpha for overlap visibility.
**Outcome:** Eye diagram with 20 overlaid traces, each representing one bit period with raised-cosine transitions plus Gaussian noise (sigma=0.08) and jitter (sigma=0.02). Zero defects.

---

## What Was Built
Viewport 700x500. Eye diagram with 20 overlaid traces, each representing one bit period with raised-cosine transitions plus Gaussian noise (sigma=0.08) and jitter (sigma=0.02). All traces drawn with alpha=0.35 cyan, creating density-dependent brightness in the overlapping eye opening. 30 points per trace, 29 segments each.

| DrawItem | Element | Pipeline | Count | Color |
|---|---|---|---|---|
| 102-159 | 20 traces | lineAA@1 | 29 seg each | cyan 35% alpha |

Total: 63 unique IDs (1 pane, 1 layer, 1 transform, 20 buffers, 20 geometrys, 20 drawItems).

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
- **Eye opening is visible at the center of the diagram where traces separate into high/low states.** 
- **Low alpha (0.35) creates additive density: areas with more overlapping traces appear brighter.** 
- **Noise and jitter blur the transition edges, creating the characteristic soft eye boundary.** 
- **All four transition types (0→0, 0→1, 1→0, 1→1) are represented across the random traces.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Eye diagrams overlay many bit-period traces — low alpha enables density visualization through additive blending.** 
2. **Raised cosine transitions model realistic signal bandwidth limiting in digital communications.** 
