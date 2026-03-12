# Trial 044: Step Chart

**Date:** 2026-03-12
**Goal:** Step-line pricing chart with 10 price points connected by 19 horizontal-then-vertical line segments (lineAA@1, rect4), 3 annotation bands (instancedRect@1, alpha 0.08) for budget/standard/premium tiers, a reference line at Y=40, and grid lines. Tests step-line tessellation pattern, layered annotation bands, and data-space coordinate correctness across a 1000×600 viewport.
**Outcome:** All 19 step segments correctly connected with zero breaks. Horizontal/vertical alternation is perfect (10H + 9V). All 3 annotation bands at correct Y ranges with contiguous stacking. Reference line centered in standard band. Transform math exact. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**Step line (1 lineAA@1 DrawItem, rect4, 19 instances):**
Blue (#3b82f6), lineWidth 2, alpha 1.0. 10 price points connected by horizontal-then-vertical segments:

| Point | X | Price ($) |
|-------|---|-----------|
| 1 | 1 | 29 |
| 2 | 3 | 35 |
| 3 | 5 | 39 |
| 4 | 8 | 29 |
| 5 | 10 | 35 |
| 6 | 12 | 45 |
| 7 | 15 | 49 |
| 8 | 18 | 39 |
| 9 | 20 | 45 |
| 10 | 22 | 55 |

Pattern: horizontal segment holds previous price, then vertical segment transitions to new price. 10 horizontal + 9 vertical = 19 total segments.

**3 annotation bands (3 instancedRect@1 DrawItems, rect4, 1 instance each):**

| Band | Y Range | Color | Alpha |
|------|---------|-------|-------|
| Budget | 25–35 | Emerald (#10b981) | 0.08 |
| Standard | 35–45 | Amber (#f59e0b) | 0.08 |
| Premium | 45–55 | Pink (#ec4899) | 0.08 |

All span X=[0, 25]. Contiguous: Budget top (35) = Standard bottom (35), Standard top (45) = Premium bottom (45).

**Reference line (1 lineAA@1 DrawItem, rect4, 1 instance):**
White, alpha 0.25, lineWidth 1. From (0, 40) to (25, 40) — centered in the Standard band.

**Grid lines (2 lineAA@1 DrawItems, rect4):**
White, alpha 0.06, lineWidth 1.
- Horizontal: 4 lines at Y=20, 30, 40, 50
- Vertical: 5 lines at X=5, 10, 15, 20, 24

Data space: X=[0, 25], Y=[0, 60]. Transform 50: sx=0.076, sy=0.031667, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Bands (11) → Reference (12) → StepLine (13).

Total: 27 unique IDs.

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

- **All 19 step segments perfectly connected.** Every segment endpoint equals the next segment's start point. All 18 junctions verified with zero gaps.

- **Horizontal/vertical alternation is correct.** 10 horizontal segments (Y constant) alternate with 9 vertical segments (X constant). The pattern starts horizontal (hold previous price) then vertical (transition to new price), which is the standard step-chart convention.

- **All 10 price points at correct positions.** The step line traces through all 10 (X, Price) data points, with each horizontal hold extending to the next transition X.

- **Price trend is visible.** Overall upward trend from $29 to $55, with two notable drops (point 4: $39→$29, point 8: $49→$39). The step pattern clearly shows discrete pricing changes.

- **All 3 annotation bands at correct Y ranges.** Budget [25,35], Standard [35,45], Premium [45,55] — each 10 data units tall. All span the full X range [0,25].

- **Bands are contiguous with no gaps.** Budget top (Y=35) = Standard bottom (Y=35), Standard top (Y=45) = Premium bottom (Y=45). No overlap, no gaps between tiers.

- **Reference line centered in Standard band.** Y=40 is the midpoint of [35,45]. Spans X=[0,25] covering the full data range.

- **Layer ordering is correct.** Grid (10, back) → Bands (11) → Reference (12) → StepLine (13, front). The step line draws over everything, bands draw over grid lines, reference line draws over bands. This prevents any visual occlusion of the primary data.

- **Transform math is exact.** sx=1.9/25=0.076 maps X=[0,25] to clip[−0.95,0.95]. sy=1.9/60≈0.031667 maps Y=[0,60] to clip[−0.95,0.95]. tx=ty=−0.95 correctly positions the origin.

- **All data within pane bounds.** Step line X=[1,24], Y=[29,55]. Clip range X=[−0.874,0.874], Y=[−0.032,0.792]. Well within pane limits [−0.95,0.95].

- **Grid lines at sensible intervals.** Horizontal: every 10 data units (Y=20,30,40,50). Vertical: every 5 data units (X=5,10,15,20) plus data endpoint at X=24.

- **Band alpha creates subtle background tinting.** At alpha 0.08, the bands provide visual context without overwhelming the step line data. The three colors (emerald, amber, pink) are distinct but non-distracting.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, instancedRect@1 uses rect4 ✓.

- **All vertex counts match.** StepLine: 76/4=19 ✓. Each band: 4/4=1 ✓. ReferenceLine: 4/4=1 ✓. HGrid: 16/4=4 ✓. VGrid: 20/4=5 ✓.

- **All 27 IDs unique.** No collisions across buffers, transforms, panes, layers, geometries, and drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Step charts use horizontal-then-vertical segment pairs.** Each price transition requires two line segments: a horizontal segment that holds the previous value, then a vertical segment that jumps to the new value. This creates the characteristic staircase pattern. The total segment count is always 2N−1 for N data points.

2. **Annotation bands are simple instancedRect@1 rectangles with low alpha.** A single rect4 instance per band (xMin, yMin, xMax, yMax) creates a full-width tinted region. Alpha 0.08 provides subtle context without competing with the data line.

3. **Contiguous bands need matching boundaries.** When stacking bands (Budget top = Standard bottom = 35), the boundaries must be exactly equal. Any gap or overlap would be visible as a dark line or double-tinted seam.

4. **Layer ordering separates visual concerns.** Grid (back) → bands → reference → data (front) ensures the primary data is never occluded. Each visual element type gets its own layer for clean z-ordering.

5. **Step charts work well with lineAA@1 instanced segments.** Each segment is one rect4 instance (x1,y1,x2,y2). No special tessellation needed — the engine's line rendering handles horizontal and vertical segments equally well.
