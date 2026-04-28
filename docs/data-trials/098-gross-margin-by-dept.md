# Trial 098: Gross Margin by Department

**Date:** 2026-03-22
**Goal:** For each dept: (revenue - COGS) / revenue. Department-colored bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
dept_revenue = SUM(lineTotal) GROUP BY departmentId
dept_cogs = SUM(products.unitCost * quantity) GROUP BY departmentId
margin = (revenue - cogs) / revenue
```

Two-table join: sale_items x products. Both revenue and cost computed from same join.

## Data Insight

| Department | Revenue | COGS | Margin |
|------------|---------|------|--------|
| Lumber & Building Materials | $110,695 | $69,628 | 37.1% |
| Tools & Hardware | $314,325 | $180,580 | 42.5% |
| Plumbing | $100,060 | $57,942 | 42.1% |
| Electrical | $128,326 | $72,738 | 43.3% |
| Paint & Coatings | $122,628 | $60,666 | 50.5% |
| Garden & Outdoor | $180,956 | $102,102 | 43.6% |
| Home Décor | $72,258 | $36,466 | 49.5% |
| Seasonal & Holiday | $186,928 | $102,998 | 44.9% |

Margin variation across departments reveals which categories are most profitable per
dollar of revenue. Departments with lower margins may need pricing review.

---

## What Was Built

1000x500 viewport, 8 department-colored bars showing gross margin percentage.

Total: 27 unique IDs.
