# Trial 100: Flow Diagram

**Date:** 2026-03-22
**Goal:** Left-to-right flow with 4 rounded nodes connected by 3 arrows.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x400 viewport with one pane.

Four rounded rectangles (Input → Process → Validate → Output) connected by gray horizontal lines with triangular arrowheads. Nodes are centered vertically at y=0 with 0.28×0.25 clip-space dimensions. Arrow lines bridge the gaps between nodes. Nodes on layer 10 (behind), arrows on layer 11 (on top).

Total: 9 unique IDs (1 pane, 2 layers, 3×(buf+geo+di)=6, 0 transforms)

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
- **Node spacing.** Centers at -0.65, -0.2, 0.25, 0.7 with width 0.28 leaves ~0.17 gap for arrows.
- **Arrow alignment.** Lines at y=0 connect right edge of each node to left edge of the next.
- **Arrowheads.** Small 0.04×0.06 triangles pointing right at each target node edge.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use cornerRadius for node boxes.** cornerRadius=10.0 creates professional rounded rectangles.
2. **Separate nodes and connectors onto different layers.** Controls draw order explicitly.
