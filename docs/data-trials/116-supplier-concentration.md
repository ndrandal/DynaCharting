# Trial 116: Supplier Concentration (Pareto)

**Date:** 2026-03-22
**Goal:** Cumulative % of total PO cost by supplier. Pareto chart: bars + cumulative line.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
purchase_orders GROUP BY supplierId → SUM(totalCost) AS supplierCost
ORDER BY supplierCost DESC
cumulative_pct = running_sum / total
JOIN suppliers ON id → name
```

Two-table join: purchase_orders (429) x suppliers (12).

## Data Insight

| Supplier | PO Cost | Cumulative % |
|----------|---------|-------------|
| GreenGrow Distributors | $93,048 | 14.5% |
| National Tool Distributors | $77,892 | 26.7% |
| ElectroPro Wholesale | $69,088 | 37.4% |
| ColorMaster Paints | $59,830 | 46.7% |
| Apex Tool Group | $55,375 | 55.4% |
| SeasonAll Supply | $52,270 | 63.5% |
| ProPlumb Supply | $50,191 | 71.4% |
| Pacific Lumber Co | $49,375 | 79.0% |
| SafeGuard Products | $45,554 | 86.1% |
| HomeStyle Imports | $35,860 | 91.7% |
| Midwest Fastener Supply | $35,202 | 97.2% |
| BuildRight Materials | $17,826 | 100.0% |

Total procurement: $641,513. Classic Pareto: a few suppliers account for the
majority of procurement spend, revealing supply chain concentration risk.

---

## What Was Built

1000x500 viewport, 12 blue bars (individual cost) + red cumulative line.
Two layers: bars behind, line on top. Shared transform.

Total: 10 unique IDs.
