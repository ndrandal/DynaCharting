# Trial 225: Dashboard Widget

**Date:** 2026-03-22
**Goal:** Single dashboard card component with sparkline (12 points), trend arrow, and value divider. Tests card layout with mixed pipelines.
**Outcome:** Card with 11-segment sparkline, trend triangle, and divider line. 15 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Card background | instancedRect@1 | 1 |
| 105 | 11 | Sparkline | lineAA@1 | 11 segs |
| 108 | 11 | Trend arrow | triSolid@1 | 1 tri |
| 111 | 11 | Divider line | lineAA@1 | 1 seg |

Total: 15 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Sparkline shows upward trend matching the trend arrow.
- Card has rounded corners for modern UI appearance.
- Layout elements spaced within card bounds.

### Done Wrong

Nothing.
