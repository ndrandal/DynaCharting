# Trial 044: Gold-Tier Customer — Monthly Revenue

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter sales where customerId is in gold-tier customer set. 62 gold customers, 21 months.
**Data Insight:** Gold-tier customers generate avg $6,765/month (total $142,061). Loyalty program's top tier drives consistent high-value revenue.

## Filter Logic

```
gold_ids = {c['id'] for c in db.customers if c['tier'] == 'gold'}
gold_monthly[sale['date'][:7]] += sale['total'] for gold customers
```

## Files

- `044-gold-customer-revenue.json` — SceneDocument
- `044-gold-customer-revenue.md` — this file
