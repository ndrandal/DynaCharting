# Trial 012: Gauge Cluster

**Date:** 2026-03-12
**Goal:** Three side-by-side semicircular gauge dials with gradient-colored value arcs, tick marks, and needles. First trial with horizontal multi-pane layout, arc-band tessellation, and per-pane aspect correction for taller-than-wide panes.
**Outcome:** Visually striking — the three gauges look professional. Arc geometry is circular, gradient colors are exact, needle positions are correct. Zero major defects.

---

## What Was Built

A 1200×600 viewport with three side-by-side panes:

**Pane 1 — CPU (384×564px, 73%):**
**Pane 2 — Memory (384×564px, 85%):**
**Pane 3 — Disk (384×564px, 42%):**

Each pane contains 4 DrawItems:
1. **Background track** (`triSolid@1`, pos2_clip, 180 vertices = 30 segments): full 180° semicircular band from left (180°) to right (0°), dark gray (#333340 alpha 0.4). Outer radius 1.151 data-X / 0.573 data-Y, inner radius 0.948 / 0.473.
2. **Value arc** (`triGradient@1`, pos2_color4): partial arc filled proportional to score with per-vertex gradient green (0%) → yellow (50%) → red (100%). CPU: 132 vertices (22 segments), Memory: 156 (26 segments), Disk: 78 (13 segments).
3. **Tick marks** (`lineAA@1`, 11 segments): radial lines at 0%, 10%, ..., 100% extending from just outside the outer radius. lineWidth 1.5.
4. **Needle** (`lineAA@1`, 1 segment): white line from center (0,0) to 95% of outer radius at the value angle. lineWidth 2.5.

Aspect correction: 564/384 = 1.469. X data radius = Y data radius × 1.469. Pixel radius: 170px in both axes.

Data space: [-1.3, 1.3] × [-0.6, 1.3] per pane. Static viewports (no pan/zoom).

Layout: 12px margins, 12px gaps between panes. Total: 12 + 384 + 12 + 384 + 12 + 384 + 12 = 1200 ✓.

Total resources: 3 panes, 12 layers, 3 transforms, 12 buffers, 12 geometries, 12 drawItems, 3 viewports = 54 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **lineAA@1 drawItems declare rect4 format.** Tick marks and needles (6 drawItems) use rect4 instead of pos2_clip. Same persistent issue from trials 009-011. The rendering works correctly — the engine handles the mismatch gracefully.

2. **Text labels invisible in PNG capture.** Known limitation. Without labels, the three gauges are unidentified (no "CPU 73%", "Memory 85%", "Disk 42%") and the tick marks have no percentage values.

3. **Gauges are vertically centered in tall panes.** Each pane is 564px tall but the semicircle only occupies the upper ~200px (170px radius + some tick mark extension). The lower ~350px of each pane is empty dark space. The gauges could be positioned lower in the pane, or the pane could be shorter, to reduce the dead space.

---

## Spatial Reasoning Analysis

### Done Right

- **Arcs are perfectly circular.** Verified: pixel radius X = 1.151 × 147.7 = 170.0px, pixel radius Y = 0.573 × 296.8 = 170.1px. Match within 0.1px. The aspect correction factor (1.469) correctly compensates for the 384×564px pane dimensions.

- **Arc vertex positions are exact.** Checked vertex at 174° (6° from start): data position (-1.1447, 0.0599) matches expected (1.151 × cos(174°), 0.573 × sin(174°)) to 4 decimal places.

- **Gradient color at 73% is exact.** CPU value arc endpoint color: (0.900, 0.551, 0.177). Expected: R=0.9 (flat in yellow→red range), G = 0.85 - 0.46×0.65 = 0.551, B = 0.2 - 0.46×0.05 = 0.177. All three channels match.

- **All three needle endpoints are correct.** Verified against formula `(0.95 × r_x × cos(θ), 0.95 × r_y × sin(θ))` where θ = π × (1 - score/100):
  - CPU (73°): (0.7231, 0.4081) vs expected (0.7231, 0.4083) — match within 0.0002 ✓
  - Memory (85%): (0.9743, 0.2470) vs (0.9743, 0.2471) ✓
  - Disk (42%): (-0.2719, 0.5270) vs (-0.2719, 0.5272) ✓

- **Value arc segment counts match scores.** CPU 73% → 22 segments (73% of 30), Memory 85% → 26 (85% of 30 = 25.5 → 26), Disk 42% → 13 (42% of 30 = 12.6 → 13). All rounded up correctly.

- **Layout is pixel-perfect.** Three 384px panes + 2×12px gaps + 2×12px margins = 1200px ✓. All three panes have identical clip regions offset horizontally.

- **All 54 IDs unique.** Systematic allocation: panes 1-3, layers 10-33, transforms 50-52, buffers/geometries/drawItems in 100/200/300 series with decade offsets per pane.

### Done Wrong

- **Pane height is excessive for semicircular content.** The semicircle (170px radius) plus tick extension (~20px) needs ~190px of vertical space above center + some padding below. The 564px pane height wastes ~60% of vertical space below the gauge. A 350px pane height would be more appropriate.

---

## Lessons for Future Trials

1. **Arc-band tessellation with aspect correction works well.** For a band between inner and outer radii: x = r × aspect × cos(θ), y = r × sin(θ). Verify circularity by computing pixel radii from both axes — they must match.

2. **Per-vertex gradient on arcs creates smooth color transitions.** Mapping angular position to a multi-stop color ramp (green→yellow→red) produces intuitive gauge visualizations. The triGradient@1 pipeline handles this elegantly.

3. **Size panes to content.** Don't make panes taller or wider than the content requires. For semicircular gauges, the pane height should be roughly (radius + margin), not double that. Extra empty space detracts from the visualization.

4. **Horizontal multi-pane layout is straightforward.** Compute each pane's pixel X range, convert to clip space, verify the gaps and margins sum correctly. This trial demonstrates 3 panes side-by-side for the first time.
