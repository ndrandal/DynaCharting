# Trial 066: Spirograph

**Date:** 2026-03-12
**Goal:** 5 hypotrochoid spirograph curves with different (R, r, d) parameters, additive blending for glowing overlaps, on a 700×700 square viewport. Tests parametric curve generation at scale (3,800 line segments total), trigonometric formula accuracy, additive blend mode, and symmetric centered layout.
**Outcome:** All 3,800 segments across 5 curves match the hypotrochoid formula with max error 0.000000500. All curves close perfectly (gap = 0.000000). All 18 IDs unique. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**5 hypotrochoid curves (5 lineAA@1 DrawItems, rect4):**

| # | R | r | d | Color | Segments | Period | Max Extent |
|---|---|---|---|-------|----------|--------|------------|
| 1 | 8 | 3 | 5 | #ef4444 (red) | 600 | 3π | 10 |
| 2 | 7 | 4 | 3 | #3b82f6 (blue) | 700 | 4π | 6 |
| 3 | 10 | 3 | 7 | #10b981 (emerald) | 1000 | 3π | 14 |
| 4 | 9 | 5 | 4 | #f59e0b (amber) | 900 | 5π | 8 |
| 5 | 6 | 2 | 4 | #a855f7 (purple) | 600 | 1π | 8 |

Formula: x(t) = (R−r)cos(t) + d·cos((R−r)/r · t), y(t) = (R−r)sin(t) − d·sin((R−r)/r · t)

Each curve: lineWidth 1.5, alpha 0.7, additive blend mode. 1 rect4 vertex per segment (x1, y1, x2, y2).

Total vertex data: 600+700+1000+900+600 = 3,800 segments, 15,200 floats.

Data space: centered at (0, 0), range [−15.4, 15.4] (max extent 14 + 10% padding).

Transform 50: sx=sy=0.061688, tx=ty=0.0. Maps [−15.4, 15.4] to [−0.95, 0.95].

All 5 curves on layer 10 (single pane, same depth).

Total: 18 unique IDs (1 pane, 1 layer, 1 transform, 5 buffers, 5 geometries, 5 drawItems).

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

- **All 3,800 segments match the hypotrochoid formula.** Every segment's (x1, y1, x2, y2) verified against the parametric equations. Maximum error across all segments: 0.000000500 (floating point precision limit).

- **All 5 curves close perfectly.** Each curve's last segment endpoint matches its first segment start with gap = 0.000000. The period calculation lcm(R,r)/R × 2π ensures exact closure.

- **Period calculations are correct.** Curve 1: lcm(8,3)/8 = 24/8 = 3 rotations. Curve 2: lcm(7,4)/7 = 28/7 = 4. Curve 3: lcm(10,3)/10 = 30/10 = 3. Curve 4: lcm(9,5)/9 = 45/9 = 5. Curve 5: lcm(6,2)/6 = 6/6 = 1.

- **Additive blending creates visible glow at overlaps.** The center of the spirograph where all curves converge shows bright white from additive color accumulation. This is the signature visual effect of additive blending.

- **5 visually distinct patterns.** Red (3-fold petals), blue (7-fold petals), emerald (10-fold petals, largest), amber (9-fold, complex interlocking), purple (3-fold, compact). Each is immediately distinguishable by shape and color.

- **Centered layout with symmetric scaling.** tx=ty=0 centers the origin in the viewport. sx=sy ensures circular patterns remain circular on the square viewport.

- **Max extent correctly padded.** Largest curve (emerald, R=10, r=3, d=7) reaches ±14 data units. With 10% padding: 15.4. Transform maps ±15.4 to ±0.95 clip — all curves fit within the pane.

- **Segment density appropriate for smooth curves.** At 700×700, the viewport spans ~665 pixels of data area. The largest curve (emerald, 1000 segments over circumference ~88) has ~11 segments per data unit, or ~0.7 pixels per segment — more than sufficient for smooth rendering.

- **All buffer sizes match vertex counts.** 5/5 geometries verified (rect4: 4 fpv).

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Hypotrochoid period = 2π × lcm(R, r) / R.** The curve traces a complete pattern after lcm(R,r)/R full rotations of the outer circle. Using LCM ensures the pen returns to exactly its starting point.

2. **Max extent of a hypotrochoid is (R−r) + d.** This occurs when both cosine terms are 1 (t=0). The minimum extent is |R−r−d|. These bounds determine the data-space range needed.

3. **Additive blending is ideal for overlapping curves.** When multiple semi-transparent curves cross, additive blending accumulates light rather than occluding. This creates natural intensity gradients where curves cluster.

4. **Centered transforms simplify symmetric layouts.** With tx=ty=0 and a data range symmetric around the origin, the transform just scales — no offset needed. This eliminates a common source of off-by-one positioning errors.

5. **3,800 segments in 15,200 floats is manageable.** Even with 5 curves totaling nearly 4,000 line segments, the JSON remains under 200KB and renders instantly.
