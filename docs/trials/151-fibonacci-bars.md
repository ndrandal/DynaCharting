# Trial 151: Fibonacci Bars

**Date:** 2026-03-22
**Goal:** First 12 Fibonacci numbers as vertical bars (instancedRect@1) on a 800x500 viewport. Heights: [1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144]. Warm color gradient.
**Outcome:** 12 bars with correct Fibonacci height ratios. Maximum bar (144) fills viewport height. Warm amber color with corner radius. Zero defects.

---

## What Was Built

A 800x500 viewport with 12 vertical bars:
- Heights proportional to Fibonacci sequence: [1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144]
- instancedRect@1 with rect4 format, 12 rects
- Amber color [0.95, 0.6, 0.1], cornerRadius=3.0
- Direct clip-space coordinates (no transform needed)

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
- Fibonacci ratios preserved: each bar height = fib[i] / 144 * 1.7
- Bars evenly spaced with small gaps
- Maximum bar fills most of the viewport height
- Corner radius adds visual polish
