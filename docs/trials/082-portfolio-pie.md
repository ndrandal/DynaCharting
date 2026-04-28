# Trial 082: Portfolio Pie Chart

**Date:** 2026-03-22
**Goal:** Pie chart with 6 sectors using triSolid@1 center-fan tessellation.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

700x700 viewport with a single pane.

Six pie sectors representing portfolio allocation: Stocks (35%), Bonds (25%), Real Estate (15%), Commodities (10%), Crypto (8%), Cash (7%). Each sector is built from 24 center-fan triangles for smooth arcs. Radius 0.75 in clip space, centered at origin.

Total: 20 unique IDs (1 pane, 1 layer, 6×(buf+geo+di)=18)

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
- **Sector angles.** Fractions sum to 1.0 (100%), so sectors form a complete circle without gaps.
- **Center-fan tessellation.** 24 triangles per sector gives ~15° angular resolution, smooth arcs.
- **Clip-space radius.** r=0.75 fits within the [-0.95,0.95] pane region with margin.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Use enough segments for smooth arcs.** 24 segments per sector is adequate; fewer would show visible edges.
2. **Sum fractions to 1.0.** Pie sectors must sum to exactly 100% or a gap/overlap will appear.
