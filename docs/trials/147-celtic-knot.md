# Trial 147: Celtic Knot

**Date:** 2026-03-22
**Goal:** 4-lobe Celtic knot pattern on a 700x700 viewport. Parametric curve r = R + A*cos(4t) with triple-line rendering (main + inner + outer parallels) to simulate braided rope. Center decoration circle.
**Outcome:** 4 DrawItems. Knot curve with 60 segments per trace, 3 parallel lines for braided effect. Zero defects.

---

## What Was Built

A 700x700 viewport with a 4-lobed rose-knot curve:
- Main curve: gold (lineWidth=5), R=25, A=15, 60 segments
- Inner/outer parallel curves: darker gold (lineWidth=2), offset +/-2.5 from main
- Center circle: R=10, 32 segments

Data space: [-45,45]x[-45,45]. Transform 50: sx=sy=0.02.

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
- Parametric curve r=25+15*cos(4t) produces correct 4-lobe rose pattern
- Parallel curves at constant radial offset create braided appearance
- All lineAA@1 segments use rect4 format with correct vertex counts
- Curve is closed (last point connects to first)
