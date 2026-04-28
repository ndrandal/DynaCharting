# Trial 222: Pac-Man Scene

**Date:** 2026-03-22
**Goal:** Pac-Man with open mouth, 2 red ghosts with eyes, and 10 pellet dots. Dark blue background. Tests triSolid@1 complex shapes and points@1.
**Outcome:** Pac-Man (28 segment mouth arc), 2 ghosts with eyes, 10 dots. 19 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Pac-Man | triSolid@1 | 28 tris |
| 105 | 10 | Ghost bodies | triSolid@1 | 2 ghosts |
| 108 | 11 | White eyes | triSolid@1 | 4 circles |
| 111 | 12 | Dark pupils | triSolid@1 | 4 circles |
| 114 | 10 | Pellet dots | points@1 | 10 pts |

Total: 19 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Pac-Man mouth opens rightward at 35 degrees (total 70 deg opening).
- Ghost shapes include semicircle top, rectangular body, and wavy bottom edge.
- Ghost eyes layered correctly: white base, dark pupil on top.

### Done Wrong

Nothing.
