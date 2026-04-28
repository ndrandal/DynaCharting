# Data Trial 179: Standard Deviation Band
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Band chart showing mean +/- 1 standard deviation. Requires computing rolling statistics and building a triangulated band between upper and lower bounds.
**Goal:** Monthly revenue line with translucent band showing 3-month rolling volatility.
**Outcome:** triSolid@1 band (108 vertices) + lineAA@1 center (18 segments). 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x600. Two layers: translucent blue band (alpha 0.2) behind solid blue mean line.
Band is tessellated as two triangles per interval segment.

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
- Revenue variance is relatively consistent — the band width stays stable.
- The band effectively communicates the typical month-to-month variation range.

---
## Lessons
1. Band/ribbon charts require two-triangle-per-segment tessellation between upper and lower bounds.
2. Low alpha on the band (0.2) with solid center line creates a clear Bollinger-band-like effect.
