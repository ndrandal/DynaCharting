# Trial 062: Popular Products (Popularity > 70) — Scatter

**Date:** 2026-03-22
**Chart Type:** points@1 scatter plot
**Pipeline:** see below
**Data Points:** 33

**Query:** Filter products where popularity > 70. 33 out of 150 products qualify.
**Data Insight:** High-popularity products cluster in 431-77152 revenue range. Popularity doesn't strictly predict revenue.

## Filter Logic

```
popular = [p for p in products if p['popularity'] > 70]
Scatter: popularity (x) vs revenue (y)
```

## Files

- `062-popular-products.json` — SceneDocument
- `062-popular-products.md` — this file
