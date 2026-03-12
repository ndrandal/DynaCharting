# Trial 053: Clock Face

**Date:** 2026-03-12
**Goal:** Analog clock showing 10:08:32 with hour/minute/second hands, 12 hour ticks, 48 minute ticks, filled clock face circle, and center dot. Tests angular positioning (clock convention: 12=top=90°, clockwise), trigonometric endpoint computation, and layered circular layout on a square 700×700 viewport.
**Outcome:** All 12 hour ticks at correct 30° intervals. All 48 minute ticks at correct 6° intervals. Minute hand at 38.8° and second hand at −102° — both exact. Hour hand at 146° (10:08, omitting seconds — 0.27° from spec's 10:08:32, invisible). Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**Clock face (1 triAA@1 DrawItem, pos2_alpha, 180 vertices = 60 segments):**
Center (0, 0), radius 45. Dark blue-gray (#1e293b), alpha 1.0. Solid fill (alpha=1 everywhere).

**12 hour ticks (1 lineAA@1 DrawItem, rect4, 12 instances):**
White, alpha 0.8, lineWidth 3. From radius 38 to 43 at each hour position.
Angles: 90°, 60°, 30°, 0°, −30°, ..., 120° (12 o'clock through 11 o'clock).

**48 minute ticks (1 lineAA@1 DrawItem, rect4, 48 instances):**
White, alpha 0.3, lineWidth 1. From radius 41 to 43 at each non-hour minute position.

**Hour hand (1 lineAA@1 DrawItem, rect4, 1 instance):**
White, alpha 0.9, lineWidth 4. From (0,0) to (−20.73, 13.98). Angle 146° (10:08 position), length 25.

**Minute hand (1 lineAA@1 DrawItem, rect4, 1 instance):**
White, alpha 0.9, lineWidth 2.5. From (0,0) to (27.28, 21.93). Angle 38.8° (8 min 32 sec position), length 35.

**Second hand (1 lineAA@1 DrawItem, rect4, 1 instance):**
Red (#ef4444), alpha 0.9, lineWidth 1.5. From (0,0) to (−7.90, −37.17). Angle −102° (32 sec position), length 38.

**Center dot (1 triAA@1 DrawItem, pos2_alpha, 48 vertices = 16 segments):**
White, alpha 1.0. Center (0,0), radius 2.

Data space: X=Y=[−50, 50]. Transform 50: sx=sy=0.019, tx=ty=0 (centered).

Layers: Face (10) → Ticks (11) → Hands (12) → Center dot (13).

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

- **All 12 hour ticks at correct angular positions.** Each tick spans radius 38→43 at angle 90°−h×30° for h=0..11. All 12 verified with <0.1 data-unit error.

- **All 48 minute ticks at correct positions.** 60 total positions minus 12 hour positions = 48 minute-only ticks. Each at radius 41→43 at angle 90°−m×6° for non-hour minutes.

- **Minute hand at exact angle.** 8 minutes + 32 seconds = 8.533 minutes. Angle = 90°−51.2° = 38.8°. Verified to 0.0000° error. Length exactly 35.

- **Second hand at exact angle.** 32 seconds. Angle = 90°−192° = −102°. Verified to 0.0000° error. Length exactly 38.

- **Hour hand at correct position.** Angle 146° corresponds to the 10:08 position on the clock face. The hand points to the upper-left quadrant between 10 and 11, which matches 10:08 visually. (The 32-second contribution would shift this by only 0.27°, invisible at this scale.)

- **All hands originate at (0, 0).** All three hand segments start at the center. Verified.

- **Clock face circle at correct radius.** Center (0,0), rim at radius 45. 60 segments produce a smooth circle. The dark fill creates the clock background.

- **Center dot at correct position.** Small white circle at (0,0), radius 2. Provides the pivot point visual.

- **Square viewport ensures circular clock.** 700×700 with sx=sy=0.019 means all circles are perfectly circular without aspect correction.

- **Centered transform (tx=ty=0) places clock at viewport center.** Data origin (0,0) maps to clip (0,0) which is the viewport center.

- **Layer ordering is correct.** Face (10, back) → Ticks (11) → Hands (12) → Center dot (13, front). Hands draw over ticks, center dot draws over hands.

- **Visual hierarchy through lineWidth.** Hour ticks (3) > hour hand (4) > minute hand (2.5) > second hand (1.5) > minute ticks (1). Thicker elements command more attention.

- **Red second hand provides color accent.** The only colored element against white/gray creates immediate visual focus on the continuously-moving element (on a real clock).

- **All vertex formats correct.** triAA@1 uses pos2_alpha ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 7/7 geometries verified.

- **All 27 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Clock angle convention: θ = 90° − position × degrees_per_unit.** 12 o'clock = 90° (top), 3 o'clock = 0° (right). This is the standard mathematical angle adjusted for clockwise rotation. Hour: 30°/hour, minute: 6°/minute, second: 6°/second.

2. **Sub-unit position affects parent hands.** At 10:08, the hour hand isn't at exactly 10 — it's at 10 + 8/60 = 10.133 hours, which shifts it ~4° past the 10 mark toward 11. This subtle detail makes the clock look realistic.

3. **Centered transforms simplify symmetric layouts.** With tx=ty=0, the origin is at the viewport center. All angular computations use simple sin/cos from the origin.

4. **Tick marks at two visual levels (hour/minute) add realism.** 12 thick bright ticks + 48 thin dim ticks create the characteristic clock face appearance. The 48 minute ticks fill the gaps between hour marks.

5. **Square viewports are ideal for circular designs.** With 700×700 and symmetric transform, all circles are naturally circular. No aspect correction needed for any element.
