# Trial 171: Pentagram

**Date:** 2026-03-22
**Goal:** 5-pointed pentagram inscribed in circle on a 700x700 viewport. Star fill (triSolid@1, 10 triangles) + inscribing circle (lineAA@1, 64 segments). Vertices at 72-degree intervals.
**Outcome:** 2 DrawItems: star fill (gold) + circle outline (grey). Inner radius computed from pentagram geometry. Zero defects.

---

## What Was Built

A 700x700 viewport with pentagram:
- 5 outer vertices at R=32.0, 72-degree intervals starting from top
- 5 inner vertices at R=12.223 (pentagram inner intersections)
- 10 triangles (fan from center through alternating outer/inner points)
- Inscribing circle: R=32.0, 64 segments

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
- Vertex spacing: exactly 72 degrees (360/5)
- Inner radius from pentagram geometry: r*sin(pi/10)/sin(7pi/10)
- Star point at top (pi/2 starting angle)
- All 5 star points touch the inscribing circle
