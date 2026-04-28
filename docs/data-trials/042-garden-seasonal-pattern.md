# Trial 042: Garden & Outdoor — Seasonal Revenue Pattern

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** department_monthly_revenue() filtered to deptId==6 (Garden & Outdoor). 21 months.
**Data Insight:** Clear spring/summer peak: 2025-05 at $12,928. Winter trough: 2025-01 at $5,745. Ratio: 2.3x.

## Filter Logic

```
dept_monthly = db.department_monthly_revenue()
garden = [r for r in dept_monthly if r['deptId'] == 6]
```

## Files

- `042-garden-seasonal-pattern.json` — SceneDocument
- `042-garden-seasonal-pattern.md` — this file
