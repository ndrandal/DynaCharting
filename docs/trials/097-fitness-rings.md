# Trial 097: Fitness Rings

**Date:** 2026-03-22
**Goal:** 3 concentric progress arcs at 75%, 50%, 90% completion with gap at top.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

600x600 viewport with one pane.

Three concentric ring arcs, each with a dim background track and bright progress arc:
- **Move** (outer, red, 75%) -- radius 0.7
- **Exercise** (middle, green, 50%) -- radius 0.5
- **Stand** (inner, cyan, 90%) -- radius 0.3

Ring width 0.12 clip units. Arcs start from top (π/2) going counterclockwise. 5% gap at top for visual separation. Background tracks rendered at 20% brightness.

Total: 21 unique IDs (1 pane, 2 layers, 6×(buf+geo+di)=18, 0 transforms)

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
- **Ring spacing.** Radii at 0.7, 0.5, 0.3 with ring width 0.12 leaves 0.08 gap between rings.
- **Layer ordering.** Tracks on layer 10, progress arcs on layer 11 ensures bright arcs overlay dim tracks.
- **Arc tessellation.** 30-40 segments per arc provides smooth curves.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Render background tracks behind progress arcs.** Dim tracks give context for incomplete progress.
2. **Use ring_arc_tris for donut-shaped arcs.** Inner/outer radius creates the ring band.
