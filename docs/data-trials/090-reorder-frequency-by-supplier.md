# Trial 090: Reorder Frequency by Supplier

**Date:** 2026-03-22
**Goal:** Count purchase orders per supplier. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId
COUNT(*) AS poCount
ORDER BY poCount DESC
JOIN suppliers ON id for name
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | PO Count |
|----------|----------|
| GreenGrow Distributors | 54 |
| ColorMaster Paints | 53 |
| ProPlumb Supply | 52 |
| ElectroPro Wholesale | 52 |
| Pacific Lumber Co | 43 |
| HomeStyle Imports | 43 |
| Midwest Fastener Supply | 30 |
| SafeGuard Products | 29 |
| National Tool Distributors | 28 |
| SeasonAll Supply | 16 |
| Apex Tool Group | 15 |
| BuildRight Materials | 14 |

Suppliers with high PO frequency serve fast-moving departments. Those with fewer POs
may supply specialty items with longer reorder cycles.

---

## What Was Built

1000x500 viewport, 12 orange bars for PO count by supplier.

Total: 6 unique IDs.
