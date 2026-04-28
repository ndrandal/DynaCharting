# Trial 104: Top 5 Products per Department

**Date:** 2026-03-22
**Goal:** For each of 8 departments, show top 5 products by revenue. 8 groups of 5 bars = 40 bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
GROUP BY (departmentId, productId) → SUM(lineTotal) AS productRevenue
For each department: ORDER BY productRevenue DESC LIMIT 5
```

Two-table join with per-group ranking.

## Data Insight

40 bars organized in 8 color-coded groups. Each group's bars are sorted by revenue,
revealing the revenue concentration within each department. Some departments rely heavily
on a single top product; others have more balanced top-5 distributions.

---

## What Was Built

1400x500 viewport, 40 bars grouped by department (5 per group, department-colored).
Each bar is a separate DrawItem for per-department palette coloring.

Total: 123 unique IDs.
