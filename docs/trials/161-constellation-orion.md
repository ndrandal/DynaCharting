# Trial 161: Constellation Orion

**Date:** 2026-03-22
**Goal:** Orion constellation on a 700x800 viewport. 7 major stars (points@1, varying pointSize) + 9 connecting lines (lineAA@1). Dark blue background.
**Outcome:** 2 DrawItems: 7 star points + 9 constellation lines. Stars in approximate Orion positions (Betelgeuse, Bellatrix, belt trio, Saiph, Rigel). Zero defects.

---

## What Was Built

A 700x800 viewport with Orion constellation:
- 7 stars: Betelgeuse (shoulder), Bellatrix (shoulder), Mintaka/Alnilam/Alnitak (belt), Saiph (foot), Rigel (foot)
- Stars rendered as points@1, pointSize=5.0, warm white
- 9 connecting lines: lineAA@1, blue-grey alpha 0.6
- Deep navy background

---

## Defects Found

### Critical
None.

### Major
None.

### Minor
None.

---

## Spatial Reasoning Analysis

### Done Right
- Star positions approximate real Orion layout
- Belt stars aligned in a near-straight line
- Betelgeuse upper-left, Rigel lower-right (correct orientation)
- Lines drawn behind stars (layer 10 < layer 11)
