# Trial 165: Mondrian Grid

**Date:** 2026-03-22
**Goal:** Piet Mondrian style composition on a 700x700 viewport. 12 rectangles (instancedRect@1) in white, red, blue, yellow. Black grid lines (lineAA@1, lineWidth=4).
**Outcome:** 13 DrawItems: 12 colored rectangles + 1 grid line set (11 segments). Classic De Stijl composition. Zero defects.

---

## What Was Built

A 700x700 viewport with Mondrian composition:
- 12 rectangles: 5 white, 3 red, 2 blue, 2 yellow
- Black grid lines: lineWidth=4, creating characteristic heavy divisions
- Asymmetric layout true to Mondrian's style
- White background

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
- Asymmetric composition: larger areas balanced by color intensity
- Primary colors only (red, blue, yellow) plus white
- Heavy black grid lines characteristic of De Stijl
- No equal-sized rectangles — deliberate asymmetry
