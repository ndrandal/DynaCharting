# Trial 103: Zone Revenue Heatmap

**Date:** 2026-03-22
**Goal:** Sum revenue by floor zone (sale_items -> products -> departments -> zones). Spatial heatmap.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId
departments.floorZoneId → zone assignment
GROUP BY floorZoneId → SUM(lineTotal)
zones → position coordinates
```

Four-table chain: sale_items -> products -> departments -> zones.

## Data Insight

| Zone | Name | Revenue |
|------|------|---------|
| A | Front (Registers & Service) | $0 |
| B | Left Wing (Lumber & Building) | $110,695 |
| C | Center Aisles (Tools/Plumbing/Electrical) | $542,711 |
| D | Right Wing (Paint & Décor) | $194,885 |
| E | Garden Center (Outdoor) | $367,884 |

The spatial heatmap reveals which physical areas of the store generate the most revenue.
High-traffic zones justify premium product placement and additional staffing.

---

## What Was Built

800x800 viewport, 5 zone rectangles positioned per floor plan coordinates.
Color intensity maps from low (yellow) to high (red) revenue via heat palette.
No transform needed — positions pre-mapped to clip space.

Total: 17 unique IDs.
