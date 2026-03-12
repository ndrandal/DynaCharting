# Trial 077: Analog Clock

**Date:** 2026-03-12
**Goal:** Analog clock face showing 10:10:30 on a 600×600 square viewport. Tests clock angle math (clockwise from 12 o'clock → standard math angle conversion), 7 distinct elements at different radii, mixed pipelines (lineAA@1 for lines, triSolid@1 for center dot), and precise trigonometric positioning of 63 distinct angular elements.
**Outcome:** All 3 hand positions match computed angles with 0.000000 error. All 12 hour markers and 48 minute ticks at correct positions (max error 0.000001). Outer ring at R=45 with 64 segments, center dot at R=2 with 16 triangles. 26 unique IDs. Zero defects.

---

## What Was Built

A 600×600 viewport (square) with a single pane (background #0f172a):

**7 DrawItems across 3 layers:**

| DrawItem | Layer | Element | Pipeline | Segments/Tris | Color | lineWidth |
|----------|-------|---------|----------|---------------|-------|-----------|
| 102 | 10 | Outer ring | lineAA@1 | 64 segs | #94a3b8 | 2.0 |
| 105 | 10 | Hour markers (×12) | lineAA@1 | 12 segs | #e2e8f0 | 3.0 |
| 108 | 10 | Minute ticks (×48) | lineAA@1 | 48 segs | #64748b | 1.5 |
| 111 | 11 | Hour hand | lineAA@1 | 1 seg | #e2e8f0 | 4.0 |
| 114 | 11 | Minute hand | lineAA@1 | 1 seg | #e2e8f0 | 2.5 |
| 117 | 11 | Second hand | lineAA@1 | 1 seg | #ef4444 | 1.5 |
| 120 | 12 | Center dot | triSolid@1 | 16 tris | #e2e8f0 | — |

**Hand positions at 10:10:30:**

| Hand | Clock angle | Math angle θ | End position | Length |
|------|-----------|-------------|--------------|--------|
| Hour | 305° | −215° | (−18.021, 12.619) | R=22 |
| Minute | 60° | 30° | (27.713, 16.000) | R=32 |
| Second | 180° | −90° | (0.000, −36.000) | R=36 |

Clock angle formula: clock_angle = value × degrees_per_unit. Conversion: θ = 90° − clock_angle. Endpoint: (R·cos θ, R·sin θ).

Data space: [−50, 50] × [−50, 50]. Transform 50: sx=sy=0.02, tx=ty=0.0. Clip range: [−0.9, 0.9].

Total: 26 unique IDs (1 pane, 3 layers, 1 transform, 7 buffers, 7 geometries, 7 drawItems).

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

- **All 3 hand endpoints match computed angles with 0.000000 error.** Hour hand at clock angle 305° (10h10m × 30°/h), minute hand at 60° (10m × 6°/m), second hand at 180° (30s × 6°/s). Each verified against θ = 90° − clock_angle conversion.

- **Second hand points straight down at :30.** Clock angle 180° → θ = −90° → (0, −36). The straight-down direction is the most visually verifiable position.

- **Hour hand between 10 and 11.** At 10:10, the hour hand has advanced 10/60 = 1/6 of the way from 10 to 11 (305° vs pure 10 at 300°). Visually confirmed in PNG.

- **All 12 hour markers at correct radial positions.** Inner radius R=40, outer radius R=44. Each marker is a radial line at h × 30° from 12 o'clock. All 12 verified with 0.000 error.

- **All 48 minute ticks at correct positions.** Non-hour minutes (every minute except 0, 5, 10, ..., 55) at R=42 to R=44. Maximum error: 0.000001 across all 48 ticks.

- **Outer ring traces full 360° at R=45.** 64 segments, all vertices at R=45.000000. Ring closes perfectly (gap = 0.000000).

- **Center dot at R=2 with 16-segment tessellation.** Center-fan triangles, all centers at origin, all edge vertices at R=2.000000.

- **Three-layer separation.** Layer 10 (marks) renders behind layer 11 (hands) renders behind layer 12 (center dot). This ensures the center dot overlaps the hand origins.

- **Visual hierarchy: hands distinguishable by width and color.** Hour hand (4.0px white), minute hand (2.5px white), second hand (1.5px red). The red second hand stands out clearly.

- **All buffer sizes match vertex counts.** 7/7 geometries verified.

- **All 26 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Clock angle to math angle: θ = 90° − clock_angle.** Clock angles go clockwise from 12 o'clock (north). Standard math angles go counterclockwise from 3 o'clock (east). The conversion θ = 90° − α maps between them.

2. **Hour hand advances continuously.** At h hours and m minutes, clock_angle = (h + m/60) × 30°. The m/60 fraction prevents the hand from snapping to integer hours.

3. **48 + 12 = 60 tick marks total.** 12 hour markers at R=40→44 (thick) + 48 minute ticks at R=42→44 (thin). The hour markers are longer (4 units vs 2 units) for visual distinction.

4. **lineAA@1 rect4 with vertexCount=1 renders a single line segment.** Each vertex stores (x1, y1, x2, y2) — both endpoints. This is the most compact way to draw a single line.

5. **Center dot on highest layer covers hand intersection.** The small circle at R=2 hides where all three hands converge, creating a clean pivot point.
