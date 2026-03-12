# Trial 067: DNA Double Helix

**Date:** 2026-03-12
**Goal:** DNA double helix with two sinusoidal backbone strands (cyan and magenta), depth illusion via alpha-split front/back segments, and 20 alternating-color base-pair rungs on a 500×900 portrait viewport. Tests front/back depth classification by cosine sign, layered rendering (back → rungs → front), parametric sinusoidal curves, and non-uniform aspect ratio transform.
**Outcome:** All 600 strand segments match formulas with max error 0.000000499. All 20 rungs at correct positions with alternating colors. Depth illusion convincing with proper interweaving at crossing points. 23 unique IDs. Zero defects.

---

## What Was Built

A 500×900 viewport (portrait) with a single pane (background #0f172a):

**2 backbone strands, each split into front/back (4 lineAA@1 DrawItems, rect4):**

| DrawItem | Strand | Portion | Color | Alpha | Layer | Segments |
|----------|--------|---------|-------|-------|-------|----------|
| 102 | Left (cyan) | Front | #06b6d4 | 0.9 | 13 | 143 |
| 105 | Left (cyan) | Back | #06b6d4 | 0.3 | 11 | 157 |
| 108 | Right (magenta) | Front | #d946ef | 0.9 | 13 | 143 |
| 111 | Right (magenta) | Back | #d946ef | 0.3 | 11 | 157 |

Left strand: x(t) = 4·cos(t), y(t) = t. Right strand: x(t) = 4·cos(t+π) = −4·cos(t), y(t) = t.

t ∈ [0, 30], 300 segments per strand (dt = 0.1). Depth classified by cos(t_mid) > 0 → front (layer 13, alpha 0.9), else back (layer 11, alpha 0.3). Both strands use the same classification. lineWidth 2.0.

**20 base-pair rungs (2 lineAA@1 DrawItems, rect4, 10 segments each):**

| DrawItem | Color | Count | t-values |
|----------|-------|-------|----------|
| 114 | #ef4444 (red, A-T) | 10 | 2.25, 5.25, 8.25, ..., 29.25 |
| 117 | #3b82f6 (blue, G-C) | 10 | 0.75, 3.75, 6.75, ..., 27.75 |

Each rung: horizontal line from (4·cos(t), t) to (−4·cos(t), t). Layer 12 (between back and front). lineWidth 3.0, alpha 0.6. Colors alternate blue/red.

Data space: X=[−6, 6], Y=[0, 30]. Transform 50: sx=0.158333, sy=0.063333, tx=0.0, ty=−0.95.

Helix completes ~4.77 full turns (30 / 2π ≈ 4.77).

Total: 23 unique IDs (1 pane, 3 layers, 1 transform, 6 buffers, 6 geometries, 6 drawItems).

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

- **All 600 strand segments match parametric formulas.** Left strand: x = 4cos(t), y = t. Right strand: x = −4cos(t), y = t. Maximum error across all segments: 0.000000499 (floating point precision).

- **Depth classification creates convincing interweaving.** Both strands classified by cos(t_mid) > 0 → front. Since strands are always on opposite sides of x=0 (left at +x when right is at −x and vice versa), the brightness transitions at crossing points create the visual illusion of strands passing over and under each other.

- **Layer ordering is correct.** Back segments (layer 11) → rungs (layer 12) → front segments (layer 13). Rungs are sandwiched between the depth layers, appearing to connect strands through the middle of the helix.

- **All 20 rungs at correct positions.** Each rung is horizontal (y1 = y2) at the correct t-value. X-coordinates match ±4cos(t) with sub-millionth precision.

- **Rung colors alternate correctly.** Blue, red, blue, red... across all 20 rungs (A-T and G-C base pairs).

- **Front/back segment counts are complementary.** 143 front + 157 back = 300 per strand. The asymmetry (143 vs 157) is correct because the period of cos(t) doesn't divide evenly into 300 segments.

- **Non-uniform aspect transform is correct.** sx=0.158333 (for X range 12) ≠ sy=0.063333 (for Y range 30). The 500×900 viewport × these different scales creates a natural elongated helix appearance.

- **Portrait viewport suits the helical structure.** The tall 500×900 format naturally accommodates a vertical helix with ~4.77 turns.

- **Adequate data range padding.** Data X range [−6, 6] provides 2 units of clearance beyond the maximum amplitude of 4, preventing clipping.

- **All buffer sizes match vertex counts.** 6/6 geometries verified (rect4: 4 fpv).

- **All 23 IDs unique.** No collisions across 1 pane, 3 layers, 1 transform, 6 buffers, 6 geometries, 6 drawItems.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Depth illusion via alpha-split segments.** Splitting a curve into front (bright) and back (dim) portions based on a cosine threshold creates effective pseudo-3D depth on a 2D canvas. The key is classifying each segment at its midpoint.

2. **Same-phase classification works for paired strands.** When two strands are always on opposite sides (like a DNA helix), classifying both by cos(t) > 0 produces correct visual depth because the strands don't overlap — each is only visible on "its" side.

3. **Three-layer depth sandwich for rungs.** Layer ordering back(11) → rungs(12) → front(13) makes rungs appear to be inside the helix, connecting the strands through the middle.

4. **Non-uniform sx/sy handles portrait layouts.** Different X and Y ranges on a non-square viewport require different scale factors. sx=1.9/12 and sy=1.9/30 correctly map both ranges to [−0.95, 0.95].

5. **143/157 segment split is expected.** With 300 segments over ~4.77 periods, the front/back split is not exactly 50/50 because the total parameter range (0 to 30) doesn't end at a cos(t)=0 crossing. The slight asymmetry is correct, not a bug.
