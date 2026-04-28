# Trial 100: Supplier Reliability

**Date:** 2026-03-22
**Goal:** For each supplier: avg actual lead time vs expected leadTimeDays. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders: (receivedDate - orderDate) AS actualLeadTime
GROUP BY supplierId → AVG(actualLeadTime)
JOIN suppliers ON id → leadTimeDays
Plot: (expected, actual) per supplier
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | Expected | Avg Actual |
|----------|----------|-----------|
| Pacific Lumber Co | 7d | 7.9d |
| Midwest Fastener Supply | 5d | 5.9d |
| National Tool Distributors | 4d | 5.0d |
| ProPlumb Supply | 5d | 5.6d |
| ElectroPro Wholesale | 4d | 5.4d |
| ColorMaster Paints | 6d | 6.9d |
| GreenGrow Distributors | 3d | 4.0d |
| HomeStyle Imports | 10d | 10.5d |
| SafeGuard Products | 6d | 7.1d |
| BuildRight Materials | 8d | 9.5d |
| Apex Tool Group | 5d | 6.3d |
| SeasonAll Supply | 7d | 7.8d |

Points on the diagonal mean the supplier delivers on time. Points above the diagonal
deliver late; below means early. Consistent lateness is a supply chain risk.

---

## What Was Built

800x600 viewport, 12 amber scatter points. X = expected lead time, Y = actual avg lead time.

Total: 6 unique IDs.
