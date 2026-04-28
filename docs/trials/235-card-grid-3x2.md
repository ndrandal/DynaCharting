# Trial 235: Card Grid 3x2

**Date:** 2026-03-22
**Goal:** 6 cards in 3x2 grid. Each card has shadow, body, and colored header stripe. Tests card UI pattern with shadow effect.
**Outcome:** 6 cards with 6 unique header colors and offset shadow rects. 28 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Shadows | instancedRect@1 | 6 |
| 105 | 11 | Card bodies | instancedRect@1 | 6 |
| 108-125 | 12 | Header stripes | instancedRect@1 | 6 |

Total: 28 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Shadow offset creates depth effect.
- Header stripes color-coded per card.
- Grid layout with consistent gaps.

### Done Wrong

Nothing.
