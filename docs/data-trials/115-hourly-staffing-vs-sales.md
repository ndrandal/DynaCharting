# Trial 115: Hourly Staffing vs Sales

**Date:** 2026-03-22
**Goal:** By hour: average staff on shift vs average sales count. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
shifts: for each (hour, date), count employees on shift
  → AVG across dates per hour → avgStaff
sales: for each (hour, date), count transactions
  → AVG across dates per hour → avgSales
Plot: (avgStaff, avgSales) per hour
```

Two-table temporal cross-reference: shifts (13,751) x sales (12,338).

## Data Insight

18 hours plotted (5 AM - 10 PM). The scatter shows whether staffing levels
match sales demand at each hour. Points below the ideal line indicate understaffing;
points above indicate overcapacity.

---

## What Was Built

800x600 viewport, 18 blue scatter points. X = avg staff, Y = avg sales.

Total: 6 unique IDs.
