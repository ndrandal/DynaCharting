# Trial 048: Top 10 Products by Profit Margin

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 10

**Query:** product_rankings() sorted by margin desc, top 10. Margin = (unitPrice - unitCost) / unitPrice.
**Data Insight:** Highest margin: Teflon Tape 1/2in Roll at 59.7%. Top 10 margins range 54.5% to 59.7%.

## Filter Logic

```
products = db.product_rankings()
by_margin = sorted(products, key=lambda r: -r['margin'])[:10]
```

## Files

- `048-top-10-by-margin.json` — SceneDocument
- `048-top-10-by-margin.md` — this file
