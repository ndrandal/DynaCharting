# Trial 162: Geometric Flag

**Date:** 2026-03-22
**Goal:** Abstract geometric flag on a 900x600 viewport. 3 horizontal bands (red, white, green) + central blue circle + gold 5-pointed star.
**Outcome:** 5 DrawItems: 3 band rects + 1 circle (32 tris) + 1 star (10 tris). Clean layered composition. Zero defects.

---

## What Was Built

A 900x600 viewport with:
- Layer 10: 3 horizontal bands (instancedRect@1): green bottom, white middle, red top
- Layer 11: Blue circle (triSolid@1), R=0.25 in clip space
- Layer 12: Gold 5-pointed star (triSolid@1) inside circle
- Direct clip-space coordinates (no transform)

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
- Bands divide viewport into equal thirds
- Circle centered at origin overlaps middle band
- Star centered inside circle with outer/inner radius ratio ~2.5
