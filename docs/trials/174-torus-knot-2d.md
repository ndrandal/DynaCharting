# Trial 174: Torus Knot 2D

**Date:** 2026-03-22
**Goal:** Projected trefoil knot on a 700x700 viewport. Parametric: x=cos(t)+2cos(2t), y=sin(t)-2sin(2t). 200 segments. lineAA@1.
**Outcome:** 1 DrawItem with 200 line segments. Classic trefoil knot (3-lobed closed curve). Cyan on dark background. Zero defects.

---

## What Was Built

A 700x700 viewport with trefoil knot:
- Parametric curve: x = cos(t) + 2*cos(2t), y = sin(t) - 2*sin(2t)
- t from 0 to 2*pi, 200 segments
- Scaled by 10x for data space, transform sx=sy=0.028
- lineAA@1, lineWidth=3.0, cyan color

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
- Trefoil knot parametric equations correctly implemented
- 3-fold rotational symmetry visible in output
- Curve is closed (start and end points match)
- 200 segments provides smooth curve
