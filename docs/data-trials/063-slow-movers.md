# Trial 063: Slow-Moving Products (< 100 Units Sold)

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars (top 20 shown)
**Pipeline:** see below
**Data Points:** 4

**Query:** product_rankings() filtered to units < 100. 4 products qualify out of 150.
**Data Insight:** Slowest: Portable Generator 3500W with 34 units. These products may need markdown, removal, or repositioning.

## Filter Logic

```
slow = [r for r in product_rankings() if r['units'] < 100]
slow.sort(key=lambda r: r['units'])
```

## Files

- `063-slow-movers.json` — SceneDocument
- `063-slow-movers.md` — this file
