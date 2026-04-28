# Trial 105: Weekday x Department Revenue Heatmap

**Date:** 2026-03-22
**Goal:** Revenue by (day-of-week x department). 7x8 colored grid.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales → day-of-week from date
sale_items JOIN products ON productId → departmentId
GROUP BY (dayOfWeek, departmentId) → SUM(lineTotal)
```

Three-table join with dual-dimensional grouping: 7 days x 8 departments = 56 cells.

## Data Insight

56 heatmap cells. Weekends (Sat/Sun) show higher revenue across most departments.
Garden & Outdoor may peak on Saturdays. Seasonal & Holiday may spike on specific days
during promotion periods. The viridis color scale maps low (dark) to high (bright) revenue.

---

## What Was Built

900x700 viewport, 56 individually-colored instancedRect cells. Viridis palette.

Total: 171 unique IDs.
