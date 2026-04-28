# Data Trial 175: Simulated Log Scale
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** The engine has no log scale — prices range from $0.79 to $899.99 (1000x range). Solution: pre-compute log10(price) in Python and plot that.
**Goal:** Products on scatter: X=units sold, Y=log10(price). Simulated logarithmic Y axis.
**Outcome:** 150 products plotted with log-transformed Y. Reference lines at $1, $10, $100. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x700. points@1 scatter with log-transformed Y values.
Dashed horizontal reference lines mark $1, $10, and $100 price points.
Text labels show actual dollar values at reference lines.

The log transformation is done entirely in Python — the engine sees linear values.

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
- Price range spans 3 orders of magnitude: $0.79 to $899.99.
- Low-price items tend to have higher unit sales — classic price-volume relationship.
- Log scale reveals the distribution that would be crushed by linear scaling.

---
## Lessons
1. Log scale can be simulated by pre-transforming data — the engine does not need native log support.
2. Reference lines at decade points ($1, $10, $100) provide essential context for log-scaled axes.
