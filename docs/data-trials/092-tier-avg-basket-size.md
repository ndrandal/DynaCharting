# Trial 092: Tier Average Basket Size

**Date:** 2026-03-22
**Goal:** By customer tier: average itemCount per sale. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales JOIN customers ON customerId
GROUP BY customers.tier
AVG(sales.itemCount) AS avgBasket
```

Two-table join: sales (12,338) x customers (500).

## Data Insight

| Tier | Avg Items/Sale |
|------|----------------|
| gold | 2.78 |
| silver | 2.70 |
| bronze | 2.71 |

Gold customers buy more items per visit than silver or bronze, confirming that loyalty
tier correlates with project size (larger DIY projects = more items).

---

## What Was Built

800x500 viewport, 3 tier-colored bars (gold/silver/bronze).

Total: 12 unique IDs.
