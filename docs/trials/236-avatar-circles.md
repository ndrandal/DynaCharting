# Trial 236: Avatar Circles

**Date:** 2026-03-22
**Goal:** 5 overlapping avatar circles in a row. Each slightly overlaps the previous. Last circle is gray ("+3 more" indicator). Dark border rings create separation.
**Outcome:** 5 overlapping circles with border rings. 33 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| borders | 10 | Border rings | triSolid@1 | 5 circles |
| avatars | 11 | Colored fills | triSolid@1 | 5 circles |

Each circle = 20 triangle fan segments. Overlap = 0.07 clip units.

Total: 33 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Circles overlap left-to-right with leftmost on top.
- Dark border rings create visual separation between overlapping circles.
- Gray last circle implies more items.

### Done Wrong

Nothing.
