# Trial 085: Gold Customer Department Preference

**Date:** 2026-03-22
**Goal:** Filter sales to gold-tier customers, join to products, group by department. Pie chart.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
customers WHERE tier='gold' → gold_customer_ids
sales WHERE customerId IN gold_customer_ids → gold_sale_ids
sale_items WHERE saleId IN gold_sale_ids
  JOIN products ON productId → departmentId
GROUP BY departmentId
SUM(lineTotal)
```

Four-table chain: customers -> sales -> sale_items -> products.
Gold customers: 62 of 500.

## Data Insight

| Department | Revenue | Share |
|------------|---------|-------|
| Tools & Hardware | $33,619 | 25.9% |
| Seasonal & Holiday | $18,333 | 14.1% |
| Garden & Outdoor | $17,988 | 13.8% |
| Electrical | $16,205 | 12.5% |
| Paint & Coatings | $12,925 | 9.9% |
| Lumber & Building Materials | $12,707 | 9.8% |
| Plumbing | $10,096 | 7.8% |
| Home Décor | $8,161 | 6.3% |

Gold customers tend to spend heavily on high-ticket items (Lumber, Tools) rather than
consumables, reflecting their DIY project orientation.

---

## What Was Built

700x700 viewport, pie chart with 8 wedges, one per department.
Each wedge uses its department palette color. triSolid@1 triangle fans.

Total: 26 unique IDs.
