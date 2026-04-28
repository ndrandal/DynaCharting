# Trial 071: Supplier Cost Ranking by Total PO Cost

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 12

**Query:** supplier_performance() sorted by totalCost desc. 12 suppliers.
**Data Insight:** Top supplier: GreenGrow Distributors at $93,048 (15% of total). Concentration risk: top 3 = 37%.

## Filter Logic

```
suppliers = db.supplier_performance()
by_cost = sorted(suppliers, key=lambda r: -r['totalCost'])
```

## Files

- `071-supplier-cost-ranking.json` — SceneDocument
- `071-supplier-cost-ranking.md` — this file
