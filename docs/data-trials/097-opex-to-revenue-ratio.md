# Trial 097: Operating Expense to Revenue Ratio

**Date:** 2026-03-22
**Goal:** Monthly: total expenses / total revenue. lineAA@1 ratio trend.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses GROUP BY month → SUM(amount) AS monthlyExpense
sales GROUP BY month → SUM(total) AS monthlyRevenue
ratio = monthlyExpense / monthlyRevenue
```

Two independent aggregations (expenses, sales) joined by month.

## Data Insight

21 months plotted. Average OpEx/Revenue ratio: 2.472 (247.2%).

A ratio below 1.0 means revenue exceeds expenses. Trending downward indicates improving
operational efficiency. Spikes may correlate with seasonal expense patterns (e.g., holiday
inventory buildup).

---

## What Was Built

1000x500 viewport, red lineAA@1 trend line with 20 segments.

Total: 6 unique IDs.
