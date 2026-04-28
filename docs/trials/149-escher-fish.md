# Trial 149: Escher Fish

**Date:** 2026-03-22
**Goal:** Tessellation of interlocking fish shapes on a 700x700 viewport. 4x4 grid of simplified fish (kite body + tail). Two alternating colors. Inspired by M.C. Escher tessellations.
**Outcome:** 48 fish total (24 cyan, 24 orange). Each fish = 3 triangles. Grid tessellation with alternating orientation. Zero defects.

---

## What Was Built

A 700x700 viewport with 4x4 fish tessellation:
- Each fish: kite-shaped body (2 tris) + triangular tail (1 tri)
- Alternating colors: cyan and orange
- Alternating flip direction for interlocking effect
- Row offset for brick-like staggering

Data space: [-35,35]x[-28,28]. Transform 50: sx=sy=0.028.

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
- Fish shapes are simplified but recognizable (kite body + V tail)
- Alternating flip creates tessellation-like interlocking
- Row stagger adds visual variety
- All vertex counts divisible by 3 for triSolid@1
