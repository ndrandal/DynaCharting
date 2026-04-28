# Data Trial 190: Benchmark Comparison
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Overlaying benchmark reference lines on top of bars. Each benchmark is a short horizontal lineAA@1 segment crossing the bar.
**Goal:** Department revenue bars with simulated industry benchmark overlay.
**Outcome:** 8 bars + 8 benchmark lines. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x600. Blue instancedRect@1 bars + red lineAA@1 benchmark markers.
Benchmarks are synthetic (random 0.8-1.3x actual revenue) for demonstration.

---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---
## Data Insights
- Some departments exceed benchmark (bar above red line), others fall short.
- The overlay pattern immediately shows over/under-performance.

---
## Lessons
1. Benchmark lines are short lineAA@1 segments overlaid on bars — simple and effective.
2. Separate layers (bars behind, benchmarks in front) ensure visibility.
