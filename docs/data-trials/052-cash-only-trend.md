# Trial 052: Cash-Only Transactions — Monthly Revenue

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter sales to paymentMethod == 'cash'. 2727 cash transactions.
**Data Insight:** Cash represents $284,434 (21.4% of all revenue). Cash usage may be declining over time as digital payments grow.

## Filter Logic

```
cash = [s for s in db.sales if s['paymentMethod'] == 'cash']
monthly_rev[s['date'][:7]] += s['total']
```

## Files

- `052-cash-only-trend.json` — SceneDocument
- `052-cash-only-trend.md` — this file
