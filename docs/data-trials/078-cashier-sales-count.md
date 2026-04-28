# Trial 078: Cashier Sales Count — Transactions per Cashier

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 4

**Query:** Filter employees with role=='Cashier'. Count sales per cashier. 4 cashiers.
**Data Insight:** Top cashier: Darius J. with 514 transactions. Total cashier transactions: 1928.

## Filter Logic

```
cashiers = [e for e in db.employees if e['role'] == 'Cashier']
sales_by_cashier = Counter(s['employeeId'] for cashier sales)
```

## Files

- `078-cashier-sales-count.json` — SceneDocument
- `078-cashier-sales-count.md` — this file
