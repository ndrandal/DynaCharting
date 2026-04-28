# Data Trial 192: Price-Volume Quadrant
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Four-quadrant scatter plot with different colors per quadrant. Requires splitting data by median price and volume, creating 4 separate points@1 DrawItems.
**Goal:** BCG-style matrix: Stars (high price + high volume), Niche, Staples, Dogs.
**Outcome:** 150 products in 4 quadrants. Stars: 24, Staples: 51, Niche: 51, Dogs: 24. 19 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x700. Four color-coded points@1 DrawItems (one per quadrant).
Dashed crosshair at median price ($16.99) and median volume (337).

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
- Stars: high-priced items that also sell well — the store's strongest products.
- Staples: low-priced, high-volume items — bread and butter of the business.
- Dogs: low price AND low volume — candidates for discontinuation.

---
## Lessons
1. Quadrant charts need separate DrawItems per quadrant for distinct colors.
2. Median-based dividers give equal-count quadrants, not equal-area quadrants.
