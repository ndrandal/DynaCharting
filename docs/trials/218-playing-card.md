# Trial 218: Playing Card (Ace of Spades)

**Date:** 2026-03-22
**Goal:** Single Ace of Spades playing card. White card body with red border, black spade symbol, and corner markers.
**Outcome:** Card with spade shape (40 triangles), border, and markers. 16 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Card body | instancedRect@1 | 1 rect |
| 105 | 11 | Red border | lineAA@1 | 4 segs |
| 108 | 12 | Spade shape | triSolid@1 | 40 tris |
| 111 | 12 | Corner markers | triSolid@1 | 2 circles |

Total: 16 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Spade shape recognizable with two lobes and pointed top.
- Card proportions approximate standard playing card ratio.
- Corner markers placed in traditional positions.

### Done Wrong

Nothing.
