# Trial 229: Toggle Switches

**Date:** 2026-03-22
**Goal:** 4 toggle switches (2 on, 2 off). Pill-shaped tracks (green=on, gray=off) with white circular handles. Tests instancedRect@1 large cornerRadius.
**Outcome:** 4 toggles with correct on/off states. 12 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | On tracks | instancedRect@1 | 2 |
| 105 | 10 | Off tracks | instancedRect@1 | 2 |
| 108 | 11 | Handles | triSolid@1 | 4 circles |

Total: 12 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- On toggles have green tracks with handle on right side.
- Off toggles have gray tracks with handle on left side.
- Large cornerRadius creates pill shape.

### Done Wrong

Nothing.
