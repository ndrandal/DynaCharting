# Trial 061: Electrical Dept Products Sorted by Unit Price

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 18

**Query:** product_rankings() filtered to deptId==4, sorted by unitPrice desc. 18 products.
**Data Insight:** Price range: $0.79 to $149.99. Electrical spans from cheap wire nuts to expensive panel components.

## Filter Logic

```
elec = [r for r in product_rankings() if r['deptId'] == 4]
elec.sort(key=lambda r: -r['unitPrice'])
```

## Files

- `061-electrical-price-range.json` — SceneDocument
- `061-electrical-price-range.md` — this file
