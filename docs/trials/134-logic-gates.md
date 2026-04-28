# Trial 134: Logic Gates

**Date:** 2026-03-22
**Goal:** 3 logic gate symbols (AND, OR, NOT) drawn with lineAA@1 outlines, triSolid@1 fills, and input/output wires.
**Outcome:** Three logic gate symbols arranged horizontally. Zero defects.

---

## What Was Built
Viewport 900x400. Three logic gate symbols arranged horizontally. AND gate (white outline, curved right side), OR gate (yellow outline, curved body with pointed output), NOT gate (magenta triangle + inversion bubble). Each has input and output wires (gray lineAA@1). All drawn in clip space.

| Gate | Body | Wires | Color |
|---|---|---|---|
| AND | lineAA@1 outline | 3 lines | white |
| OR | lineAA@1 outline | 3 lines | yellow |
| NOT | triSolid fill + lineAA outline + bubble | 2 lines | magenta |

Total: 37 unique IDs (1 pane, 3 layers, 11 buffers, 11 geometrys, 11 drawItems).

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
- **AND gate has flat left side and semicircular right side, matching standard symbol.** 
- **OR gate has curved body tapering to a point on the output side.** 
- **NOT gate is a triangle with a small circle (inversion bubble) at the output.** 
- **Input/output wires are properly aligned with gate body edges for clean connections.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Logic gate symbols use lineAA@1 for outlines and triSolid@1 for fills — combining pipelines per element.** 
2. **Layer separation (wires < fill < outlines) ensures proper visual stacking.** 
