# Trial 075: Top 20 Customers by Total Lifetime Spend

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 20

**Query:** Aggregate sales.total per customerId, top 20. 500 customers with recorded purchases.
**Data Insight:** Top spender: Rebecca D. at $6,020. Top 20 = $70,813 (13.4% of tracked customer revenue).

## Filter Logic

```
cust_spend[s['customerId']] += s['total']
top_20 = sorted by spend desc[:20]
```

## Files

- `075-loyal-customers-spending.json` — SceneDocument
- `075-loyal-customers-spending.md` — this file
