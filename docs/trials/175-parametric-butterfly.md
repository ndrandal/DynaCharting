# Trial 175: Parametric Butterfly

**Date:** 2026-03-22
**Goal:** Butterfly curve on a 700x700 viewport. x = sin(t)*(e^cos(t) - 2cos(4t) - sin^5(t/12)), y = cos(t)*(...). t from 0 to 12pi. 300 segments.
**Outcome:** 1 DrawItem with 300 segments. Classic butterfly curve with bilateral symmetry. Pink on dark background. Zero defects.

---

## What Was Built

A 700x700 viewport with butterfly curve:
- Temple H. Fay's butterfly curve (1989)
- t ranges from 0 to 12*pi for complete figure
- 300 segments (lineAA@1, lineWidth=1.5)
- Pink color, dark background

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
- Correct butterfly curve formula with all 3 terms
- t range covers full period (12*pi = 6 complete loops)
- Bilateral symmetry visible (x uses sin(t), y uses cos(t))
- 300 segments adequate for smooth rendering
