# Trial 094: Payment Method by Department (Stacked Bars)

**Date:** 2026-03-22
**Goal:** For each department, show revenue split by payment method. Stacked bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
sale_items JOIN sales ON saleId → paymentMethod
GROUP BY (departmentId, paymentMethod)
SUM(lineTotal)
```

Three-table join: sale_items x products x sales.
Cross-tabulation: 8 departments x 4 payment methods.

## Data Insight

Payment methods: cash, check, credit_card, debit_card.
Some departments may show different payment preferences — high-ticket Lumber purchases
might skew toward credit cards, while small Garden purchases use more cash.

---

## What Was Built

1000x600 viewport, 8 stacked bar columns with 4 payment method segments each.

Total: 15 unique IDs.
