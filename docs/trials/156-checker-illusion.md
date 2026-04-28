# Trial 156: Checker Illusion

**Date:** 2026-03-22
**Goal:** 8x8 checkerboard with radial brightness gradient overlay creating optical illusion of curved surface on a 700x700 viewport.
**Outcome:** 64 checker squares (32 light, 32 dark) + 4 gradient overlay triangles (triGradient@1, Additive blend). Brightness peaks at center, creating the illusion that the board curves. Zero defects.

---

## What Was Built

A 700x700 viewport with:
- Layer 10: 8x8 checkerboard (instancedRect@1), light grey / dark grey
- Layer 11: 4 gradient triangles (triGradient@1, Additive blend) with alpha peaking at center
- Combined effect: center squares appear brighter, edges appear darker, creating apparent curvature

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
- Classic 8x8 checkerboard with equal-sized cells
- Additive gradient overlay creates brightness illusion without modifying squares
- Center alpha peak creates perceived curvature
