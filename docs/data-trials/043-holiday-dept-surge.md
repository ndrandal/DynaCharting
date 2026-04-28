# Trial 043: Seasonal & Holiday Dept — December Revenue Spike

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** department_monthly_revenue() filtered to deptId==8 (Seasonal & Holiday). 21 months.
**Data Insight:** December months show dramatic spikes. Peak: 2025-11 at $14,411. Holiday decoration/gift buying drives huge Q4 revenue.

## Filter Logic

```
dept_monthly = db.department_monthly_revenue()
seasonal = [r for r in dept_monthly if r['deptId'] == 8]
```

## Files

- `043-holiday-dept-surge.json` — SceneDocument
- `043-holiday-dept-surge.md` — this file
