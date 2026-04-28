# Trial 153: Julia Set

**Date:** 2026-03-22
**Goal:** 32x32 Julia set for c = -0.7 + 0.27i on a 700x700 viewport. Purple-to-yellow color scheme. 50 max iterations, 8 color buckets.
**Outcome:** 1024 rects in 7 color buckets. Julia set structure visible with characteristic connected/disconnected regions. Zero defects.

---

## What Was Built

A 700x700 viewport with 32x32 = 1024 Julia set pixels:
- c = -0.7 + 0.27i (connected Julia set near Mandelbrot boundary)
- Region: [-1.5, 1.5] x [-1.5, 1.5]
- Purple-to-yellow palette
- 7 DrawItems (one per color bucket)

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
- Julia iteration z = z^2 + c with fixed c correctly implemented
- c = -0.7 + 0.27i produces a well-known connected Julia set shape
- Symmetric coloring reflects Julia set's 180-degree rotational symmetry
