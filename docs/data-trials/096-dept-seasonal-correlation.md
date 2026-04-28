# Trial 096: Department Seasonal Correlation

**Date:** 2026-03-22
**Goal:** 8 overlaid lines showing monthly revenue for each department. Multi-line chart.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
sale_items JOIN sales (via _sale_month) → month
GROUP BY (departmentId, month) → SUM(lineTotal)
```

Two-table join with temporal aggregation. Produces 8 time series x 21 months.

## Data Insight

Each department has its own seasonal pattern. Garden & Outdoor peaks in spring/summer,
Seasonal & Holiday spikes in Q4, while Tools & Hardware stays relatively stable year-round.
Overlaying all 8 lines on shared axes reveals which departments move in concert.

---

## What Was Built

1200x600 viewport, 8 department-colored lineAA lines sharing one transform.
Each department is a separate DrawItem with its palette color.

Total: 27 unique IDs.
