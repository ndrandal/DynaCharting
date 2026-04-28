# Trial 170: Hexagram

**Date:** 2026-03-22
**Goal:** Star of David (hexagram) on a 700x700 viewport. Two overlapping equilateral triangles (triSolid@1, semi-transparent gold) with line outlines. Dark blue background.
**Outcome:** 3 DrawItems: upward triangle, downward triangle, combined outline (6 segments). Overlap creates darker hexagonal center. Zero defects.

---

## What Was Built

A 700x700 viewport with hexagram:
- Upward equilateral triangle: vertices at 90, 210, 330 degrees, R=30
- Downward equilateral triangle: vertices at 270, 30, 150 degrees, R=30
- Both filled with semi-transparent gold (alpha=0.7)
- Outlines: bright gold, lineWidth=2.5
- Dark blue background

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
- Two equilateral triangles rotated 180 degrees apart
- Semi-transparency creates darker overlap at center (hexagonal intersection)
- Outlines drawn on top layer for clean edges
