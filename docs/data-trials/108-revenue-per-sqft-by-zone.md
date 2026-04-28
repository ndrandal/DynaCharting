# Trial 108: Revenue per Square Foot by Zone

**Date:** 2026-03-22
**Goal:** Revenue / zone sqft for each zone. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId
  JOIN departments ON departmentId → floorZoneId
GROUP BY floorZoneId → SUM(lineTotal)
zones.sqft → revenue_per_sqft = revenue / sqft
```

Four-table chain: sale_items -> products -> departments -> zones.

## Data Insight

| Zone | Name | SqFt | Rev/SqFt |
|------|------|------|----------|
| A | Front (Registers & S | 3,000 | $0 |
| B | Left Wing (Lumber &  | 6,000 | $18 |
| C | Center Aisles (Tools | 7,000 | $78 |
| D | Right Wing (Paint &  | 5,000 | $39 |
| E | Garden Center (Outdo | 4,500 | $82 |
| F | Warehouse & Stockroo | 2,500 | $0 |

Revenue per square foot is the core retail real estate metric. Zones with high rev/sqft
justify their floor space; low-performing zones may benefit from re-merchandising.

---

## What Was Built

1000x500 viewport, 6 amber bars.

Total: 6 unique IDs.
