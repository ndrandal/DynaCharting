# Trial 268: Audio Mixer

**Date:** 2026-03-22
**Goal:** 8-channel audio mixer with background tracks, green level bars, red peak indicators, pan knob circles, and master fader line.
**Outcome:** 8 channels with varying levels (65%, 82%, 45%, 70%, 55%, 90%, 35%, 60%). 20 unique IDs. Zero defects.

---

## What Was Built

Viewport 700x500. Single pane with near-black background.

**5 DrawItems across 4 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Background tracks | instancedRect@1 | 8 rects | dark gray |
| 105 | 11 | Level bars | instancedRect@1 | 8 rects | green |
| 108 | 11 | Peak indicators | instancedRect@1 | 8 rects | red |
| 111 | 12 | Pan knobs | lineAA@1 | 96 segs | light gray |
| 114 | 13 | Master line | lineAA@1 | 1 seg | gray |

Each channel is 0.200 clip-space wide, meter height [-0.7, 0.55].

Total: 20 unique IDs (1 pane, 4 layers, 5 buffers, 5 geometries, 5 drawItems).

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
- **Level bars proportional to volume.** Each bar height = level * track height, creating a recognizable VU meter look.
- **Peak indicators sit on top of level bars.** Red sections above green create the classic peaked-meter appearance.
- **Pan knobs are circles above each channel.** 12-segment outlines at consistent Y position.
- **All 8 channels evenly spaced.** Consistent 0.200 width per channel.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Stacked rects for level meters.** Background + level + peak creates a three-layer meter with one DrawItem each.
2. **Circle outlines for knob controls.** lineAA@1 circle_outline gives a clean rotary control appearance.
