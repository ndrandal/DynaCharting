# Trial 036: Sankey Diagram

**Date:** 2026-03-12
**Goal:** Energy flow Sankey diagram — 4 sources (Coal, Gas, Nuclear, Renewables) connected to 4 end uses (Electricity, Industry, Heating, Transport) via 8 proportional flow bands. Tests triSolid@1 quadrilateral bands with source/destination sub-stacking, instancedRect@1 node rectangles, and flow conservation (total source height = total destination height = 94 units).
**Outcome:** All 8 flow band heights match their TWh values. Source and destination sub-stacking sums match node heights. Flow is conserved (94.00 on both sides). Zero defects.

---

## What Was Built

A 1100×600 viewport with a single pane (background #0f172a):

**4 source nodes (instancedRect@1, X=0 to 5):**
Coal [Y: 0–31.33], Gas [Y: 33.33–61.53], Nuclear [Y: 63.53–82.33], Renewables [Y: 84.33–100.00].

**4 destination nodes (instancedRect@1, X=95 to 100):**
Electricity [Y: 0–65.80], Industry [Y: 67.80–81.90], Heating [Y: 83.90–93.30], Transport [Y: 95.30–100.00].

**8 flow bands (triSolid@1, pos2_clip, 2 triangles each, alpha 0.4):**

| Flow | TWh | Source Y | Dest Y | Band Height |
|------|-----|----------|--------|-------------|
| Coal→Electricity | 150 | 0–23.50 | 0–23.50 | 23.50 |
| Coal→Industry | 50 | 23.50–31.33 | 67.80–75.63 | 7.83 |
| Gas→Electricity | 80 | 33.33–45.87 | 23.50–36.03 | 12.53 |
| Gas→Heating | 60 | 45.87–55.27 | 83.90–93.30 | 9.40 |
| Gas→Industry | 40 | 55.27–61.53 | 75.63–81.90 | 6.27 |
| Nuclear→Electricity | 120 | 63.53–82.33 | 36.03–54.83 | 18.80 |
| Renewables→Electricity | 70 | 84.33–95.30 | 54.83–65.80 | 10.97 |
| Renewables→Transport | 30 | 95.30–100.00 | 95.30–100.00 | 4.70 |

Scale: 0.15667 Y-units per TWh. 2-unit gaps between nodes.

Colors by source: Coal #64748b (slate), Gas #f97316 (orange), Nuclear #8b5cf6 (violet), Renewables #10b981 (emerald). Bands at alpha 0.4, nodes at alpha 0.9.

Data space: X=[0, 100], Y=[0, 100]. Transform: sx=sy=0.019, tx=ty=−0.95.

Total: 50 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 8 flow band heights match their TWh values.** Each band's source height and destination height equal flow × scale (0.15667). Verified all 8 flows — zero errors.

- **Source sub-stacking is continuous.** Within each source node, outgoing flows stack exactly: Coal [0→23.50→31.33], Gas [33.33→45.87→55.27→61.53], Nuclear [63.53→82.33], Renewables [84.33→95.30→100.00]. No gaps within nodes.

- **Destination sub-stacking is continuous.** Electricity receives 4 flows stacking [0→23.50→36.03→54.83→65.80]. Industry: [67.80→75.63→81.90]. Heating: [83.90→93.30]. Transport: [95.30→100.00].

- **Flow is conserved.** Total source node height = total dest node height = 94.00 units. Grand total TWh is 600, and 600 × 0.15667 = 94.00.

- **Band crossing patterns are visually correct.** Coal→Industry crosses Gas→Electricity (source above, dest below → bands must cross). Nuclear and Renewables flow down to Electricity. The crossing pattern in the PNG matches the data topology.

- **Node gaps are consistent.** 2-unit gaps between all adjacent source nodes and between all adjacent dest nodes.

- **Node rectangles are proportional.** Coal (200TWh) is the tallest source node. Electricity (420TWh) dominates the destination side. Visual proportions match.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, instancedRect@1 uses rect4 ✓.

- **All 50 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Sankey diagrams require double sub-stacking.** Each flow must be positioned within both its source node (outgoing stack) and its destination node (incoming stack). Both stacks must independently sum to their node's total height.

2. **Straight-line bands are a valid simplification.** True Sankey diagrams use curved bands (Bézier paths), but simple quadrilaterals connecting source and destination Y ranges clearly convey the flow structure. The crossing patterns still emerge naturally.

3. **Flow conservation is the key invariant.** Total source height must equal total destination height. This is automatically satisfied when using a uniform scale factor (TWh → Y-units) for both sides.

4. **Alpha 0.4 enables band overlap visibility.** Where bands cross, the transparency reveals both layers. This is essential for Sankey readability — opaque bands would completely occlude crossed flows.

5. **Node width (X=0–5 and X=95–100) provides visual anchoring.** The rectangles at each side give clear start/end points for the flow bands and provide area for text labels in the live viewer.
