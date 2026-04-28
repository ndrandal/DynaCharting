# Trial 041: Lumber & Building Materials — Monthly Revenue Trend

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** department_monthly_revenue() filtered to deptId==1 (Lumber). 21 months from 2024-07 to 2026-03.
**Data Insight:** Peak month: 2025-05 at $7,152. Lumber shows seasonal demand with spring/summer construction peaks.

## Filter Logic

```
dept_monthly = db.department_monthly_revenue()
lumber = [r for r in dept_monthly if r['deptId'] == 1]
```

## Files

- `041-lumber-dept-monthly.json` — SceneDocument
- `041-lumber-dept-monthly.md` — this file
