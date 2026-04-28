# Trial 058: Tools & Hardware Dept Products Sorted by Revenue

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 25

**Query:** product_rankings() filtered to deptId==2, sorted by revenue desc. 25 products.
**Data Insight:** Top seller: Cordless Drill/Driver 20V Kit at $77,152. Dept total: $314,325. Top 5 products account for 57% of department revenue.

## Filter Logic

```
tools = [r for r in product_rankings() if r['deptId'] == 2]
tools.sort(key=lambda r: -r['revenue'])
```

## Files

- `058-tools-products-by-revenue.json` — SceneDocument
- `058-tools-products-by-revenue.md` — this file
