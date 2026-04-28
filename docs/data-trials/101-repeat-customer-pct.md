# Trial 101: Repeat Customer Percentage

**Date:** 2026-03-22
**Goal:** Monthly: count of customers who also bought in prior month / total customers. Line.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY (month, customerId) → month_customers set
repeat_count = |month_customers[M] INTERSECT month_customers[M-1]|
total = |month_customers[M]|
repeatPct = repeat_count / total
```

Single-table temporal analysis on sales with set intersection logic.

## Data Insight

20 months plotted. Average month-over-month repeat rate: 38.6%.

A rising repeat rate indicates growing customer loyalty. Seasonal dips may correspond
to months where one-time project buyers inflate the unique customer count.

---

## What Was Built

1000x500 viewport, violet lineAA@1 trend with 19 segments.

Total: 6 unique IDs.
