# Trial 072: Low Stock Products Below Reorder Point

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars (top 20)
**Pipeline:** see below
**Data Points:** 8

**Query:** From latest inventory_snapshots per product, filter where qty <= reorderPoint * 1.2. 8 products near reorder threshold.
**Data Insight:** Most critical: Kitchen Faucet Single-Handle with 17 units (reorder at 17). These products are at or near replenishment threshold.

## Filter Logic

```
latest = most recent snapshot per product
low_stock = [s for s in latest if qty <= reorderPoint * 1.2]
```

## Files

- `072-low-stock-products.json` — SceneDocument
- `072-low-stock-products.md` — this file
