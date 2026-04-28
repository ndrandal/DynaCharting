# Trial 163: Heraldic Shield

**Date:** 2026-03-22
**Goal:** Heraldic shield on a 600x750 viewport. Shield outline (lineAA@1) + diagonal partition (gold/blue, triSolid@1) + red chevron (triSolid@1).
**Outcome:** 4 DrawItems: shield outline, gold field, blue field, red chevron. Classic heraldry composition. Zero defects.

---

## What Was Built

A 600x750 viewport with heraldic shield:
- Layer 10: Diagonal partition — gold upper-left, blue lower-right (triSolid@1)
- Layer 11: Red chevron (V-shape, 4 triangles)
- Layer 12: Shield outline (lineAA@1, gold, lineWidth=3)
- Transform 50: sx=0.025, sy=0.022

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
- Shield shape: rounded top, pointed bottom
- Per bend partition (diagonal) in gold and azure
- Chevron (V-shape) as charge element
- Outline drawn on top layer
