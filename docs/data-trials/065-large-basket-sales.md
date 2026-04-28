# Trial 065: Large Basket Sales (5+ Items) — Monthly Count

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 bar chart
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter sales where itemCount >= 5. 1855 qualifying sales (15.0%).
**Data Insight:** Large baskets indicate project shoppers buying multiple items. Avg 88.3 large-basket sales per month.

## Filter Logic

```
large = [s for s in db.sales if s['itemCount'] >= 5]
monthly_count[s['date'][:7]] += 1
```

## Files

- `065-large-basket-sales.json` — SceneDocument
- `065-large-basket-sales.md` — this file
