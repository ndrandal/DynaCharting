# Trial 224: Rubik's Cube Face

**Date:** 2026-03-22
**Goal:** 3x3 grid of colored squares simulating one face of a scrambled Rubik's cube. Black background with gaps between squares.
**Outcome:** 9 colored squares with 6 different colors (scrambled state). 33 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Black background | instancedRect@1 | 1 |
| 105-131 | 11 | Colored squares | instancedRect@1 | 9 |

Square size: 0.3 clip units, gap: 0.04.

Total: 33 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Black gaps between squares create the characteristic Rubik's cube appearance.
- Scrambled color arrangement looks realistic.
- Rounded corners on both background and individual squares.

### Done Wrong

Nothing.
