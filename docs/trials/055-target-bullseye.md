# Trial 055: Target Bullseye

**Date:** 2026-03-12
**Goal:** Archery target with 5 concentric colored rings (back-to-front layered circles), 192-segment ring separator outlines, crosshair lines, and 12 shot position dots scattered across the target. Tests concentric circle layering, alternating ring colors (blue/red/blue/red/gold), shot position plotting, and circular geometry at scale on a square 700×700 viewport.
**Outcome:** All 5 rings at exact radii (9, 18, 27, 36, 45). All 12 shot positions correct with accurate distances from center. 192 ring separator segments. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**5 concentric filled circles (5 triSolid@1 DrawItems, pos2_clip, 144 vertices = 48 segments each):**

| Ring | Radius | Color | Layer |
|------|--------|-------|-------|
| 5 (outermost) | 45 | #1e40af (dark blue) | 10 |
| 4 | 36 | #dc2626 (red) | 11 |
| 3 | 27 | #2563eb (blue) | 12 |
| 2 | 18 | #dc2626 (red) | 13 |
| 1 (bullseye) | 9 | #fbbf24 (gold) | 14 |

Back-to-front layering creates annular ring appearance. All centered at (0, 0).

**Ring separators (1 lineAA@1 DrawItem, rect4, 192 instances):**
4 circle outlines at radii 9, 18, 27, 36 (48 segments each = 192 total). White, alpha 0.3, lineWidth 1. Layer 15.

**Crosshairs (1 lineAA@1 DrawItem, rect4, 2 instances):**
Horizontal (−45, 0)→(45, 0) and vertical (0, −45)→(0, 45). White, alpha 0.15, lineWidth 1. Layer 15.

**12 shot positions (1 triAA@1 DrawItem, pos2_alpha, 576 vertices = 12 circles × 48 verts):**
White, alpha 1.0. Radius 2 data units. 16 segments per circle, center-fan tessellation. Layer 16.

| Shot | Position | Distance | Ring |
|------|----------|----------|------|
| 1 | (2, 3) | 3.6 | Bullseye |
| 2 | (−1, −2) | 2.2 | Bullseye |
| 3 | (5, 8) | 9.4 | Ring 2 |
| 4 | (−10, 4) | 10.8 | Ring 2 |
| 5 | (15, −12) | 19.2 | Ring 3 |
| 6 | (−8, −18) | 19.7 | Ring 3 |
| 7 | (22, 10) | 24.2 | Ring 3 |
| 8 | (−20, −22) | 29.7 | Ring 4 |
| 9 | (28, 15) | 31.8 | Ring 4 |
| 10 | (−30, 25) | 39.1 | Ring 5 |
| 11 | (35, −20) | 40.3 | Ring 5 |
| 12 | (−5, 42) | 42.3 | Ring 5 |

Data space: X=Y=[−50, 50]. Transform 50: sx=sy=0.019, tx=ty=0 (centered).

Total: 33 unique IDs.

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

- **All 5 rings at exact radii.** Ring 5: 45, Ring 4: 36, Ring 3: 27, Ring 2: 18, Ring 1 (bullseye): 9. All rim vertices verified at exact distance from center.

- **Back-to-front layering creates clean annular bands.** Each ring occludes the one behind it, creating the characteristic target ring pattern without polygon subtraction. Same technique proven in Trial 041 (contour map).

- **Alternating blue/red color scheme is authentic.** The dark blue → red → blue → red → gold pattern matches standard archery target coloring.

- **All 12 shot positions at correct coordinates.** Every shot's (x, y) verified against the spec. All 12/12 exact.

- **Shot distances correctly place them in expected rings.** Shots 1–2 (dist < 9) are in the bullseye. Shots 3–4 (9 < dist < 18) are in ring 2. And so on through ring 5. The scatter pattern is visually realistic.

- **192 ring separator segments define ring boundaries.** 4 circles × 48 segments = 192. These white outlines make the ring boundaries crisp against the solid fills.

- **Crosshairs provide aiming reference.** Horizontal and vertical lines spanning the full target diameter.

- **Square viewport ensures circular target.** 700×700 with sx=sy=0.019 means all circles are perfectly circular.

- **Centered transform places target at viewport center.** tx=ty=0 maps origin (0,0) to clip (0,0).

- **48 segments per ring produce smooth circles.** At 7.5° per segment, the polygonal approximation is invisible at 700×700 resolution.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, triAA@1 uses pos2_alpha ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 8/8 geometries verified.

- **All 33 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Concentric filled circles with back-to-front layering create annular rings.** This is the same proven technique from Trial 041 (contour map). Largest circle in back, smallest in front. No complex ring geometry needed.

2. **Alternating colors require separate DrawItems per ring.** Since each ring has a different color, each needs its own DrawItem. 5 rings = 5 DrawItems.

3. **Shot positions as a single DrawItem with multiple circles is efficient.** All 12 shots share the same color (white), so 576 vertices go into one triAA@1 DrawItem.

4. **Ring separators as line circles add definition.** The filled rings alone create the target appearance, but thin white outlines at the boundaries make the rings crisper and more readable.

5. **Distance-from-center determines ring assignment.** For scatter data on a target, the radial distance √(x²+y²) compared against ring boundaries categorizes each shot. This is implicit in the visual but should be verified in the audit.
