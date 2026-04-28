# Trial 107: Cumulative Customer Growth

**Date:** 2026-03-22
**Goal:** Day-by-day cumulative count of unique customers. lineAA@1.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales ORDER BY date
Running set union of customerId (non-null) per day
cumCustomers = |seen_customers| at each date
```

Single-table temporal accumulation on sales (12,338 rows).

## Data Insight

630 days plotted. Final cumulative unique customers: 500.
The growth curve shows customer acquisition rate over time. A steepening curve
indicates accelerating customer acquisition; a flattening curve suggests market saturation.

---

## What Was Built

1000x500 viewport, green lineAA@1 cumulative growth curve with 629 segments.

Total: 6 unique IDs.
