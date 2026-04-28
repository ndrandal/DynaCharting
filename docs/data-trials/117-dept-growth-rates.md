# Trial 117: Department Growth Rates

**Date:** 2026-03-22
**Goal:** Month-over-month revenue growth % by department. 8 overlaid lines.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
department_monthly_revenue() → per-dept time series
growth_pct = (current - previous) / previous * 100 for each consecutive month pair
```

Pre-aggregated query with derived calculation (percentage change).

## Data Insight

8 department growth lines over 20 month transitions.
High volatility indicates seasonal sensitivity. Departments with consistently
positive growth are the store's engines; those with negative trends need attention.

---

## What Was Built

1200x600 viewport, 8 department-colored lineAA@1 lines showing MoM growth %.

Total: 27 unique IDs.
