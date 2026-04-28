# Trial 051: Single-Item Sales — Monthly Count

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter sales where itemCount == 1. 3523 single-item sales (28.6% of all).
**Data Insight:** Single-item purchases represent quick runs — hardware store grab-and-go shoppers. Consistent 168/month baseline.

## Filter Logic

```
single = [s for s in db.sales if s['itemCount'] == 1]
monthly_count[s['date'][:7]] += 1
```

## Files

- `051-single-item-sales.json` — SceneDocument
- `051-single-item-sales.md` — this file
