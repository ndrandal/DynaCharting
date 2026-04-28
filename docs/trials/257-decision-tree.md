# Trial 257: Decision Tree

**Date:** 2026-03-22
**Goal:** Binary decision tree with 7 nodes (3 decision + 4 leaf). Leaf outcomes color-coded green (yes) / red (no). Branching lines.
**Outcome:** 3 blue decision nodes, 2 green leaves, 2 red leaves, 6 branch lines. 15 unique IDs. Zero defects.

---

## What Was Built
Viewport 700x600. 3-level binary tree. Root (y=0.7) branches to L/R (y=0.3), each branches to 2 leaves (y=-0.15).
Decision nodes: blue with cornerRadius=8. Leaf nodes: green (accept) or red (reject).
Branch lines (lineAA@1) connect parent bottom edge to child top edge.
Total: 15 unique IDs (1 pane, 2 layers, 4 buffers, 4 geometries, 4 drawItems).

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
- **Tree levels evenly spaced vertically.** y=0.7, 0.3, -0.15 gives clear hierarchy.
- **Leaf nodes at same vertical level.** All 4 leaves at y=-0.15 for visual balance.
- **Color coding communicates outcome.** Green=accept, red=reject — instantly readable.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Binary trees: each level doubles width.** Root centered, children spread equally.
2. **Three separate drawItems for three node types is clearer than triGradient@1 when the colors are categorical.**
