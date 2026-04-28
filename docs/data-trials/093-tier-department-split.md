# Trial 093: Tier x Department Revenue (Stacked Bars)

**Date:** 2026-03-22
**Goal:** For each customer tier, show revenue by department as stacked bars. 3 groups x 8 dept stacks.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales JOIN customers ON customerId → sale_tier mapping
sale_items JOIN products ON productId → departmentId
GROUP BY (tier, departmentId) → SUM(lineTotal)
```

Three-table chain: customers -> sales -> sale_items -> products.
Cross-tabulation: 3 tiers x 8 departments = 24 cells.

## Data Insight

Gold customers generate more total revenue despite being fewer in number.
Department preferences vary by tier — gold customers may over-index on Lumber
(project materials) while bronze customers buy more consumables.

---

## What Was Built

900x600 viewport, 3 stacked bar columns (gold/silver/bronze), each with 8 department
color segments. 8 series using to_stacked_bars.

Total: 27 unique IDs.
