# Trial 228: Slider Controls

**Date:** 2026-03-22
**Goal:** 3 horizontal sliders at 25%, 60%, 85%. Each has a gray track, colored fill, and white circular handle. Tests lineAA@1 tracks with triSolid@1 handles.
**Outcome:** 3 sliders with correct fill positions. 16 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Track lines | lineAA@1 | 3 segs |
| 105,108,111 | 11 | Colored fills | lineAA@1 | 3 segs |
| 108 | 12 | Handles | triSolid@1 | 3 circles |

Total: 16 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Slider handles positioned at correct percentages along tracks.
- Color-coded fills match slider identity.
- White handles provide clear affordance.

### Done Wrong

Nothing.
