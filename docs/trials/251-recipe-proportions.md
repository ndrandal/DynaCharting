# Trial 251: Recipe Proportions

**Date:** 2026-03-22
**Goal:** Horizontal bar chart of 8 recipe ingredients, color-coded by type, using triGradient@1 for per-vertex color.
**Outcome:** 8 bars with proportional lengths. 48 vertices (16 triangles). 5 unique IDs. Zero defects.

---

## What Was Built
Viewport 800x500. Dark background. 8 horizontal bars for cookie recipe ingredients.
Each bar length proportional to cups/amount. Colors: tan (flour), white (sugar), yellow (butter), amber (eggs), blue (milk), brown (vanilla), gray (baking powder, salt).
triGradient@1 with pos2_color4 gives each bar its own color without needing separate drawItems.
Total: 5 unique IDs (1 pane, 1 layer, 1 buffer, 1 geometry, 1 drawItem).

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
- **Bar lengths proportional to ingredient amounts.** Flour (2 cups) is longest, salt (0.05) shortest.
- **Consistent bar height and spacing.** 0.08 height, 0.04 gap produces readable layout.
- **triGradient@1 enables per-bar coloring in a single drawItem.** Efficient use of the pipeline.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **triGradient@1 is ideal for multi-colored bars.** One drawItem handles all bars when each vertex carries its own color.
2. **Proportional bar width = (value/max) * available_width.** Simple linear mapping.
