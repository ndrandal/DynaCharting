# Trial 112: Payroll/Revenue Ratio

**Date:** 2026-03-22
**Goal:** Monthly payroll expense / revenue. lineAA@1 trend.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses WHERE accountId IN (2,3) ('Salaries & Wages')
GROUP BY month → SUM(amount) AS payroll
sales GROUP BY month → SUM(total) AS revenue
ratio = payroll / revenue
```

Two independent aggregations (expenses filtered to payroll, sales) joined by month.

## Data Insight

21 months. Average payroll/revenue ratio: 1.781 (178.1%).

A lower ratio means each dollar of payroll generates more revenue. Seasonal spikes
(e.g., holiday staffing) may temporarily increase the ratio.

---

## What Was Built

1000x500 viewport, orange lineAA@1 with 20 segments.

Total: 6 unique IDs.
