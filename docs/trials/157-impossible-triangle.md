# Trial 157: Impossible Triangle

**Date:** 2026-03-22
**Goal:** Penrose impossible triangle on a 700x700 viewport. 3 bars at 120 degrees apart (triSolid@1 parallelograms) with inner face strips for 3D illusion. Each bar in a different color.
**Outcome:** 6 DrawItems (3 outer bars + 3 inner faces). Equilateral triangle skeleton with impossible geometry illusion. Zero defects.

---

## What Was Built

A 700x700 viewport with Penrose impossible triangle:
- 3 bars connecting equilateral triangle vertices at R=25
- Each bar = parallelogram (2 triangles) + inner face strip (2 triangles)
- Colors: blue, red, green with lighter inner faces
- Transform 50: sx=sy=0.03

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
- 120-degree angles between bars (equilateral triangle)
- Bar width creates 3D appearance
- Inner face strips with lighter color simulate depth
- Bars drawn in overlapping order to create impossible geometry
