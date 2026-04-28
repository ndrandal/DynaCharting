# Trial 239: Font Specimen

**Date:** 2026-03-22
**Goal:** 6 horizontal bars of increasing height simulating text lines at different font sizes. Light gray on dark. Left-aligned.
**Outcome:** 6 bars from smallest to largest, left-aligned. 5 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Text-like bars | instancedRect@1 | 6 |

Heights: 0.03, 0.045, 0.06, 0.08, 0.11, 0.15 clip units.

Total: 5 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Progressive height increase suggests font size hierarchy.
- Left-aligned like natural text.
- Light gray on dark background for readability.

### Done Wrong

Nothing.
