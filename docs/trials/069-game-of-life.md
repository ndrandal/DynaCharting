# Trial 069: Game of Life

**Date:** 2026-03-12
**Goal:** Conway's Game of Life at generation 60, starting from a Gosper Glider Gun at offset (1,1) on a bounded 46×36 grid. Tests cellular automaton simulation accuracy (B3/S23 rules for 60 generations), instancedRect@1 for irregular cell placement, and correct initial pattern encoding on a 900×600 viewport.
**Outcome:** All 46 live cells match an independently computed simulation exactly. The Gosper Glider Gun's oscillating mechanism, emitted glider, and still-life blocks are all present and correctly positioned. 10 unique IDs. Zero defects.

---

## What Was Built

A 900×600 viewport with a single pane (background #0f172a):

**Grid background (1 instancedRect@1 DrawItem, rect4, 1 instance):**
Single rectangle [0, 0] to [46, 36], color #1e293b, alpha 0.3, layer 10.

**46 live cells (1 instancedRect@1 DrawItem, rect4, 46 instances):**
Color #3b82f6 (blue), alpha 0.9, layer 11. Each cell 0.85×0.85, centered in a 1×1 grid slot (0.075 offset).

**Visible structures at generation 60:**
- Gosper Glider Gun oscillator (ring-shaped structure in lower-left area)
- Gun feed mechanisms (clusters around the oscillator)
- 2×2 block still life (bottom-left, part of the gun)
- 2×2 block still life (bottom-right, debris from gun operation)
- Emitted glider (5-cell pattern traveling toward upper-right)
- Intermediate debris from glider emission

Starting from 36 cells (Gosper Gun at offset (1,1)), after 60 B3/S23 generations: 46 live cells.

Data space: X=[0, 46], Y=[0, 36]. Transform 50: sx=0.041304, sy=0.052778, tx=−0.95, ty=−0.95.

Total: 10 unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems).

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

- **All 46 live cells match independent simulation.** B3/S23 rules run for 60 generations on a bounded 46×36 grid starting from the standard Gosper Glider Gun pattern at offset (1,1). Every cell position verified — zero discrepancies.

- **Gosper Glider Gun initial pattern is correct.** 36 cells in the standard pattern, including the left block pair (rows 4–5, cols 0–1), the central oscillating ring (cols 10–16), the right feed structure (cols 20–24), and the far-right block pair (rows 2–3, cols 34–35). All offset by (1,1).

- **Visible structures are physically correct for generation 60.** The gun fires one glider every 30 generations. At gen 60, two gliders have been emitted: the first has traveled ~15 cells diagonally (visible in upper-right area), the second is just emerging. The gun mechanism continues oscillating.

- **2×2 block still lives are stable.** The blocks visible at bottom-left and bottom-right are expected — they're part of the gun mechanism and debris that form stable still-life patterns.

- **Cell dimensions uniform.** All 46 cells are exactly 0.85×0.85 with 0.075 offset from grid boundaries.

- **Bounded grid simulation prevents edge artifacts.** The 46×36 grid provides adequate clearance for the gun (occupying ~37×10 cells) and emitted gliders. Cells at grid boundary are treated as dead, preventing wrap-around artifacts.

- **Non-square viewport handled correctly.** 900×600 (3:2) with different sx and sy (0.041304 vs 0.052778) correctly maps the 46×36 data space to fill the clip region.

- **All buffer sizes match vertex counts.** 2/2 geometries verified (rect4: 4 fpv).

- **All 10 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Game of Life simulation is the core verification target.** The entire trial's correctness hinges on running B3/S23 rules exactly. Independent re-simulation is the only reliable audit: re-implement the automaton from scratch and compare cell sets.

2. **Gosper Glider Gun fires every 30 generations.** At gen 60, exactly 2 gliders have been emitted. The gun mechanism is a period-30 oscillator, so the gun itself returns to its initial configuration every 30 steps.

3. **Bounded grid needs adequate clearance.** 46×36 for a gun occupying ~37×10 provides ~10 cells of clearance for glider travel. For longer simulations, the grid would need to be larger.

4. **10 IDs for a cellular automaton is minimal.** One DrawItem for all live cells (regardless of structure type) keeps the scene simple. Structure identification (gun, glider, still life) is visual, not encoded in the data.

5. **Cell placement at col+0.075 with 0.85 width creates clean gaps.** The 0.15-unit gap (0.075 on each side) between adjacent cells provides clear visual separation at the rendered cell size (~17×14 pixels).
