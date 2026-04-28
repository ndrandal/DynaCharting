# Trial 049: Bottom 10 Products by Profit Margin

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 10

**Query:** product_rankings() sorted by margin asc, bottom 10.
**Data Insight:** Lowest margin: Play Sand 50lb Bag at 30.8%. These products are sold near cost — loss leaders or commodity items.

## Filter Logic

```
products = db.product_rankings()
by_margin = sorted(products, key=lambda r: r['margin'])[:10]
```

## Files

- `049-bottom-10-by-margin.json` — SceneDocument
- `049-bottom-10-by-margin.md` — this file
