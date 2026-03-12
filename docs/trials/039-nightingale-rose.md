# Trial 039: Nightingale Rose Chart

**Date:** 2026-03-12
**Goal:** Monthly rainfall Nightingale Rose (coxcomb) chart — 12 equal-angle sectors (30° each) with radii proportional to rainfall values, creating the characteristic rose-petal pattern. Tests polar coordinate tessellation of variable-radius sectors (triSolid@1 triangle fans), concentric reference circles (lineAA@1, 72 segments each), and 12 radial separator lines from origin to r=5.5.
**Outcome:** All 12 sector radii match the formula r=rainfall/18 to zero error. All 12 sector boundaries have zero angular gap. Concentric circles have exact radii. Radial separators all reach r=5.5 at correct angles. Zero defects.

---

## What Was Built

A 750×750 viewport (square) with a single pane (background #0f172a):

**12 sectors (12 triSolid@1 DrawItems, pos2_clip, 6 triangles each):**

| Month | Rainfall (mm) | Radius | Angle Range | Color |
|-------|--------------|--------|-------------|-------|
| Jan | 78 | 4.333 | 90°→60° | #3b82f6 (blue) |
| Feb | 65 | 3.611 | 60°→30° | #60a5fa (light blue) |
| Mar | 52 | 2.889 | 30°→0° | #93c5fd (pale blue) |
| Apr | 42 | 2.333 | 0°→−30° | #10b981 (emerald) |
| May | 38 | 2.111 | −30°→−60° | #34d399 (light emerald) |
| Jun | 25 | 1.389 | −60°→−90° | #6ee7b7 (pale emerald) |
| Jul | 18 | 1.000 | −90°→−120° | #f59e0b (amber) |
| Aug | 22 | 1.222 | −120°→−150° | #fbbf24 (yellow) |
| Sep | 35 | 1.944 | −150°→−180° | #f97316 (orange) |
| Oct | 55 | 3.056 | −180°→150° | #fb923c (light orange) |
| Nov | 68 | 3.778 | 150°→120° | #ec4899 (pink) |
| Dec | 82 | 4.556 | 120°→90° | #f472b6 (light pink) |

Radius formula: r = rainfall / 18.0 (Jul=1.0 minimum, Dec=4.556 maximum). Each sector tessellated with 6 triangle-fan triangles (5° subdivisions).

**3 concentric reference circles (1 lineAA@1 DrawItem, rect4, 216 instances = 3×72):**
At r=1.5, 3.0, 4.5. White, alpha 0.08, lineWidth 1. 72 segments each (5° per segment).

**12 radial separator lines (1 lineAA@1 DrawItem, rect4, 12 instances):**
From origin (0,0) to r=5.5 at every 30° boundary. White, alpha 0.15, lineWidth 1.

Data space: X=Y=[−8, 8]. Transform 50: sx=sy=0.11875, tx=ty=0.

Layers: circles (10) → separators (11) → sectors (12).

Total: 47 unique IDs.

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

- **All 12 sector radii match the formula to zero error.** Every non-origin vertex in each sector lies at exactly r = rainfall/18.0. Maximum error across all 144 arc vertices is 0.000001 (float precision).

- **All 12 sector boundary angles have zero gap.** Verified all 12 adjacencies (Jan→Feb, Feb→Mar, ..., Dec→Jan wrap-around). The last vertex of each sector and the first vertex of the next sector lie on the same radial direction with angle difference 0.0000°.

- **Wrap-around is exact.** Dec ends at 90° (r=4.556) and Jan starts at 90° (r=4.333). The rose closes perfectly at the 12 o'clock position.

- **Sectors start at 90° (12 o'clock) and proceed clockwise.** Jan occupies 90°→60° (top-right), progressing through the right side (Feb-Mar), bottom-right (Apr-May), bottom (Jun-Jul-Aug), left (Sep-Oct), and upper-left (Nov-Dec) before wrapping back to 90°.

- **Concentric reference circles have exact radii.** All 216 segment endpoints lie at exactly r=1.5, 3.0, or 4.5 with zero error. Each circle closes perfectly (last segment endpoint = first segment startpoint).

- **All 12 radial separators start at origin and reach r=5.5.** Every separator has startpoint at (0,0) and endpoint at exactly r=5.500 at the correct 30° boundary angle.

- **Triangle fan tessellation is correct.** Each sector uses 6 triangles sharing the origin vertex, with successive rim vertices at 5° angular intervals. 6 triangles × 30° sector = 5° per triangle.

- **Visual proportions match data.** Dec (82mm, largest) has the most prominent petal. Jul (18mm, smallest) is barely visible. The seasonal pattern (wet winter, dry summer) is immediately readable from the rose shape.

- **Transform is optimally simple.** sx=sy=0.11875, tx=ty=0. Origin-centered symmetric data range maps perfectly to the square viewport.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match buffer data.** 12 sectors: 36/2=18 each ✓. Reference circles: 864/4=216 ✓. Separators: 48/4=12 ✓.

- **All 47 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Nightingale roses use constant-angle, variable-radius sectors.** Unlike pie charts (constant radius, variable angle), coxcomb charts encode data in sector radius. Equal 30° angles ensure each month gets equal angular representation, while radius (and thus area) encodes the value.

2. **Triangle fan tessellation is ideal for polar sectors.** Each triangle shares the origin vertex, with successive rim vertices along the arc. 5° subdivisions produce visually smooth arcs even for the largest sectors.

3. **12 separate DrawItems enable per-month coloring.** With triSolid@1, each DrawItem has a single color. The seasonal color gradient (blue winter → green spring → amber summer → pink autumn) requires one DrawItem per sector.

4. **Reference circles provide scale context.** Without the concentric circles, it's hard to judge absolute radius differences. The three circles at equal intervals (1.5, 3.0, 4.5) serve as ruler marks.

5. **Radial separators prevent visual merging of adjacent sectors.** Even with distinct colors, adjacent sectors of similar radius can appear to merge at their boundary. The thin white radial lines ensure clear sector delineation.
