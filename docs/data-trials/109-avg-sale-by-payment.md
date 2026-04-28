# Trial 109: Average Sale by Payment Method

**Date:** 2026-03-22
**Goal:** Average transaction total by payment method. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY paymentMethod
AVG(total) AS avgSale
```

Single-table aggregation on sales (12,338 rows).

## Data Insight

| Payment Method | Avg Transaction |
|----------------|----------------|
| check | $118.96 |
| credit_card | $109.74 |
| debit_card | $105.29 |
| cash | $104.30 |

Credit card transactions tend to have higher averages (less friction for large purchases).
Cash transactions may be smaller, reflecting impulse buys or small items.

---

## What Was Built

800x500 viewport, 4 colored bars for average sale by payment method.

Total: 15 unique IDs.
