# Trial 103: Semicircle Gauge

**Date:** 2026-03-22
**Goal:** Large semicircle gauge with red/yellow/green zones and needle at 65.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

700x500 viewport with one pane.

A semicircular gauge centered at (0, -0.15) with radius 0.75:
- **Red zone** (0-30%) on the left
- **Yellow zone** (30-70%) in the middle
- **Green zone** (70-100%) on the right

White needle line points to value 65 (in the yellow zone near green boundary). Small white center dot at the pivot point.

Total: 18 unique IDs (1 pane, 2 layers, 5×(buf+geo+di)=15)

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
- **Zone boundaries.** Red/yellow at 0.7π, yellow/green at 0.3π — each zone spans proportionally.
- **Needle angle.** Value 65 maps to 0.35π radians, correctly positioned in the yellow zone.
- **Center dot.** 16-segment circle at the pivot adds a professional finish.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Add a center dot to gauges.** The pivot point anchors the needle visually.
2. **Offset gauge center downward.** cy=-0.15 leaves room above the semicircle for labels.
