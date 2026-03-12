# Trial 027: Dot Matrix Chart

**Date:** 2026-03-12
**Goal:** GitHub-style weekly activity grid — 7 rows (Mon–Sun) × 12 columns (W1–W12) = 84 circles at 5 activity levels with a green intensity scale. Tests triAA@1 at scale (84 circles, 12,096 total vertices across 5 DrawItems), aspect-corrected circle tessellation at 0.636:1 ratio, and visual encoding via color intensity alone.
**Outcome:** All 84 circles have perfect 4px pixel radius at every angle. Aspect correction, fringe, grid positions, and color levels all verified. Zero defects.

---

## What Was Built

A 1000×400 viewport with a single pane (930×345px region, clipX [−0.91, 0.95], clipY [−0.875, 0.85]):

**84 circles across 5 activity levels (triAA@1, pos2_alpha, 144 vertices each):**

| Level | DrawItem | Color (RGBA) | Dots | Vertices |
|-------|----------|-------------|------|----------|
| 0 (inactive) | 102 | (0.086, 0.106, 0.133, 1.0) — near-background gray | 20 | 2,880 |
| 1 (low) | 105 | (0.055, 0.267, 0.161, 1.0) — dark green | 23 | 3,312 |
| 2 (medium) | 108 | (0.000, 0.427, 0.196, 1.0) — medium green | 21 | 3,024 |
| 3 (high) | 111 | (0.149, 0.651, 0.255, 1.0) — bright green | 15 | 2,160 |
| 4 (max) | 114 | (0.224, 0.828, 0.326, 1.0) — vivid green | 5 | 720 |
| **Total** | | | **84** | **12,096** |

Grid: months 0–11 (X), days 0–6 (Y). All 84 positions fill the 12×7 grid exactly — no duplicates, no gaps.

Circle radii: X=0.051613, Y=0.081159 data units. Pixel radius: 4.000px both axes. Aspect correction: 0.63595 (px_per_dy/px_per_dx = 49.286/77.5). Fringe: 2.5px. 16 angular segments per circle, 144 vertices each (48 core + 96 fringe).

Data space: X=[0, 11], Y=[0, 6]. Transform: sx=0.155, sy=−0.246429, tx=−0.8325, ty=0.726786. Negative sy maps day 0 (Mon) to the top, day 6 (Sun) to the bottom.

Layers: Level 0 (10) → Level 1 (11) → Level 2 (12) → Level 3 (13) → Level 4 (14).

Text overlay: title ("Weekly Activity"), 7 day labels (Mon–Sun), 12 week labels (W1–W12) = 20 labels.

Total: 1 pane, 5 layers, 1 transform, 5 buffers, 5 geometries, 5 drawItems = 22 IDs.

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

- **All 84 circles have exactly 4.000px pixel radius at every angle.** Verified all 16 segments of a reference circle: every core rim vertex lies exactly 4.000px from center in screen space, from 0° through 337.5°. The aspect-corrected ellipse parametrization is mathematically perfect.

- **Fringe is exactly 2.500px at every angle.** All 16 fringe outer vertices lie exactly 6.500px (4.0 + 2.5) from center. Core alpha=1.0, fringe alpha=0.0 — correct for AA fade.

- **All 84 grid positions are correct.** 12 months × 7 days = 84 positions, all filled with exactly one circle each. No duplicates, no gaps. Verified by extracting all centers and checking uniqueness.

- **Activity level distribution is plausible.** Level 0: 20, Level 1: 23, Level 2: 21, Level 3: 15, Level 4: 5. The distribution follows a roughly normal pattern with most activity at low-to-medium levels, peaking at Level 1, tapering to only 5 max-activity days. Visually realistic for an activity grid.

- **Transform correctly inverts Y axis.** sy=−0.246429 maps day 0 (Mon) to clipY=0.727 (top) and day 6 (Sun) to clipY=−0.752 (bottom). This is the standard "top-to-bottom" convention for weekly grids.

- **Text label positions match transform.** All 7 day labels (Mon–Sun) and 12 week labels (W1–W12) align exactly with the grid positions computed from the transform (verified to ≤0.001 precision). Day labels at clipX=−0.98 (left margin), week labels at clipY=−0.95 (bottom margin).

- **Color scale creates clear visual hierarchy.** The 5-level green scale from near-background (Level 0) through vivid green (Level 4) creates the classic GitHub activity heat pattern. Level 0 dots blend into the background, while Level 4 dots are immediately prominent.

- **Background color (#0d1b22) matches the dark theme.** The pane clear color (0.051, 0.067, 0.090) creates sufficient contrast for all activity levels.

- **All vertex formats correct.** triAA@1 uses pos2_alpha ✓. All 5 geometries: vertex counts match buffer sizes exactly (buffer floats ÷ 3 = vertex count).

- **All vertex counts match.** Level 0: 8640/3=2880=20×144 ✓. Level 1: 9936/3=3312=23×144 ✓. Level 2: 9072/3=3024=21×144 ✓. Level 3: 6480/3=2160=15×144 ✓. Level 4: 2160/3=720=5×144 ✓.

- **All 22 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Dot matrix charts encode information through color intensity, not position.** All dots occupy the same fixed grid; only the color level varies. This requires grouping dots by activity level into separate DrawItems (one per color), not one DrawItem per dot.

2. **5 DrawItems for 84 circles is efficient.** Rather than 84 individual DrawItems (which would consume 84 IDs each for buffer+geometry+drawItem = 252 IDs), grouping by activity level produces only 5 DrawItems with 22 total IDs. Each DrawItem's buffer concatenates all circles at that level.

3. **Aspect correction at 0.636:1 is moderate.** With px_per_dy (49.3) smaller than px_per_dx (77.5), Y data radius is ~1.57× the X data radius. This is a moderate correction — less extreme than Trial 024 (0.14:1) but still essential for circular dots.

4. **Negative sy naturally handles top-to-bottom grids.** By making sy negative, row 0 maps to the top of the viewport. This avoids manual Y-axis inversion in the data and keeps the grid intuitive (Monday at top, Sunday at bottom).

5. **Wide aspect ratio (1000×400) suits grid visualizations.** The 2.5:1 viewport ratio gives each dot ample horizontal spacing (77.5px between column centers) while keeping rows compact (49.3px between row centers). The dots don't feel cramped despite the wide layout.
