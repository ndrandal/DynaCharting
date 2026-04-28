# Trial 110: Seasonal Stockout Risk by Department

**Date:** 2026-03-22
**Goal:** Products where inventory dipped below 5 in any snapshot, count by department. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
inventory_snapshots WHERE quantityOnHand < 5 → at-risk productIds
JOIN products ON productId → departmentId
GROUP BY departmentId → COUNT(DISTINCT productId)
```

Two-table join: inventory_snapshots (3,150) x products (150).
At-risk threshold: quantityOnHand < 5.

## Data Insight

| Department | At-Risk Products |
|------------|-----------------|
| Lumber & Building Materials | 0 |
| Tools & Hardware | 0 |
| Plumbing | 0 |
| Electrical | 0 |
| Paint & Coatings | 0 |
| Garden & Outdoor | 0 |
| Home Décor | 0 |
| Seasonal & Holiday | 0 |

Total at-risk products: 0.
Departments with more stockout-risk products need tighter reorder-point management.

---

## What Was Built

1000x500 viewport, 8 department-colored bars.

Total: 27 unique IDs.
