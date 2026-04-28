# Trial 088: Staffing vs Revenue Scatter

**Date:** 2026-03-22
**Goal:** For each day, compute (total shift hours, total revenue). Scatter showing correlation.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
shifts GROUP BY date → SUM(hoursWorked) AS dayHours
sales GROUP BY date → SUM(total) AS dayRevenue
INNER JOIN on date
```

Two-table cross-join by date: shifts (13,751) and sales (12,338).

## Data Insight

630 days plotted. Average daily staffing: 157 hours, average daily revenue: $2,109.

The scatter reveals the correlation between labor investment and revenue output.
Days with more staff-hours tend to produce more revenue, but diminishing returns appear
at high staffing levels.

---

## What Was Built

800x600 viewport, 630 cyan scatter points. X = staff hours, Y = revenue.

Total: 6 unique IDs.
