# Trial 048: Calendar Heatmap

**Date:** 2026-03-12
**Goal:** GitHub-style calendar heatmap with 52 weeks × 7 days = 364 cells grouped into 5 intensity tiers (instancedRect@1 with cornerRadius). Tests high-density grid layout, deterministic pseudorandom activity generation, tier-based cell grouping, Y-axis inversion, and month separator placement on a 1000×400 viewport.
**Outcome:** All 364 cells match the activity formula exactly (46+50+53+65+150 per tier). All cell dimensions correct (0.8×0.8 with 0.2 gap). No duplicate or out-of-bounds cells. Zero defects.

---

## What Was Built

A 1000×400 viewport with a single pane (background #0f172a):

**364 day cells across 5 tiers (5 instancedRect@1 DrawItems, rect4, cornerRadius 2px):**

| Tier | Activity | Cells | Color |
|------|----------|-------|-------|
| 0 | None | 46 | #161b22 (very dark) |
| 1 | Low | 50 | #0e4429 (dark green) |
| 2 | Medium | 53 | #006d32 (green) |
| 3 | High | 65 | #26a641 (bright green) |
| 4 | Very high | 150 | #39d353 (vivid green) |

Activity formula: `floor(abs(sin(w×7 + d×13 + 42) × 5))` where w=week (0–51), d=day (0–6).

Each cell spans [w+0.1, d+0.1] to [w+0.9, d+0.9] — 0.8×0.8 data units with 0.2 gap.

**Border (1 lineAA@1 DrawItem, rect4, 4 instances):**
Rectangle [0,0]→[52,7]. White, alpha 0.1, lineWidth 1.

**Month separators (1 lineAA@1 DrawItem, rect4, 11 instances):**
Vertical lines at X=4, 9, 13, 17, 22, 26, 30, 35, 39, 44, 48. From Y=0 to Y=7. White, alpha 0.08, lineWidth 1.

Data space: X=[0, 52], Y=[0, 7]. Transform 50: sx=0.036538, sy=−0.271429, tx=−0.95, ty=0.95.

**Y-axis inversion:** Negative sy maps day 0 (Monday) to the top and day 6 (Sunday) to the bottom.

Layers: Grid (10) → Cells (11).

Total: 25 unique IDs.

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

- **All 364 cells match the activity formula exactly.** Every cell's tier was independently recomputed using `floor(abs(sin(w×7 + d×13 + 42) × 5))` and compared against the tier's buffer data. 364/364 correct with zero mismatches.

- **Tier counts are correct.** Tier 0: 46, Tier 1: 50, Tier 2: 53, Tier 3: 65, Tier 4: 150. All match the expected distribution from the pseudorandom formula.

- **All cell dimensions correct.** Every cell spans exactly [w+0.1, d+0.1] to [w+0.9, d+0.9], creating 0.8×0.8 squares with 0.2 gaps. The grid pattern is clearly visible in the PNG.

- **No duplicate cells.** All 364 cells are unique across all 5 tiers. No cell appears in more than one tier.

- **No out-of-bounds cells.** All weeks in [0,51], all days in [0,6].

- **Y-axis inversion works correctly.** Negative sy=−0.271429 places Monday (d=0) at the top and Sunday (d=6) at the bottom.

- **Transform math is exact.** sx=1.9/52=0.036538 maps X=[0,52] to clip[−0.95,0.95]. sy=−1.9/7=−0.271429 with ty=0.95 inverts Y correctly.

- **5-tier green color scheme is GitHub-authentic.** The gradient from very dark (#161b22) through dark green to vivid green (#39d353) creates the instantly recognizable contribution graph appearance.

- **Tier grouping minimizes DrawItems.** 364 cells in 5 DrawItems (one per tier) instead of 364 individual DrawItems. All cells in a tier share the same color.

- **Corner radius adds visual polish.** 2px cornerRadius creates the rounded squares characteristic of GitHub's contribution graph.

- **Border frames the grid area.** 4-segment rectangle at [0,0]→[52,7] provides a subtle boundary.

- **Month separators at approximate boundaries.** 11 vertical lines at weeks 4, 9, 13, 17, 22, 26, 30, 35, 39, 44, 48 — roughly corresponding to month transitions in a year.

- **Layer ordering is correct.** Grid/border (10, back) → Cells (11, front). Cells draw over the grid lines.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 7/7 geometries verified.

- **All 25 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Grouping hundreds of same-styled rectangles by tier is highly efficient.** 364 cells collapsed into 5 DrawItems. The engine handles large instance counts (150 instances in tier 4) without issue.

2. **Deterministic pseudorandom formulas create realistic test patterns.** `floor(abs(sin(w×7 + d×13 + 42) × 5))` produces a non-uniform distribution across 5 tiers that looks natural. The sin function with different frequency multipliers for w and d creates varied patterns.

3. **Calendar grids use (week, day-of-week) as (X, Y) coordinates.** This natural mapping makes the grid layout trivial: X position = week number, Y position = day number. Cell gaps come from undersizing each cell (0.8 vs 1.0 grid spacing).

4. **Y-axis inversion keeps the data model simple.** Rather than subtracting day indices (6−d), using negative sy in the transform maps day 0 to the top naturally. The data stays in its logical form.

5. **Corner radius on instancedRect@1 adds visual quality.** The 2px cornerRadius creates rounded squares that match modern UI aesthetics (GitHub contribution graph style). This is a single DrawItem property, no extra geometry needed.
