# Trial 233: Tooltip Callout

**Date:** 2026-03-22
**Goal:** Tooltip rectangle with triangular pointer pointing down to a target point. Dashed connector line. Tests callout UI pattern.
**Outcome:** Tooltip box, pointer triangle, target point, and dashed connector. 15 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Tooltip box | instancedRect@1 | 1 |
| 105 | 10 | Pointer triangle | triSolid@1 | 1 tri |
| 108 | 11 | Target point | points@1 | 1 pt |
| 111 | 10 | Dashed connector | lineAA@1 | 1 seg |

Total: 15 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Triangle pointer seamlessly connects to box bottom (same color).
- Dashed line visually links tooltip to target.
- Target point highlighted with large red dot.

### Done Wrong

Nothing.
