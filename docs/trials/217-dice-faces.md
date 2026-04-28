# Trial 217: Dice Faces

**Date:** 2026-03-22
**Goal:** 6 dice showing faces 1-6 in a horizontal row. Rounded rect bodies with pip dots. Tests instancedRect@1 cornerRadius and triSolid@1 small circles.
**Outcome:** 6 dice with 21 total pips. 9 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Die bodies | instancedRect@1 | 6 |
| 105 | 11 | Pip dots | triSolid@1 | 21 circles |

Each pip = 10-triangle fan. Total: 9 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Standard pip layouts for faces 1-6.
- Dice evenly spaced in horizontal row.
- Rounded corners create realistic die appearance.

### Done Wrong

Nothing.
