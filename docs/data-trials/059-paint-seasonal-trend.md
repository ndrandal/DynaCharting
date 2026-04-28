# Trial 059: Paint & Coatings — Monthly Revenue (Spring Painting Season)

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** department_monthly_revenue() filtered to deptId==5 (Paint). 21 months.
**Data Insight:** Peak: 2025-03 at $8,109. Paint sales follow spring renovation season with March-June being peak months.

## Filter Logic

```
dept_monthly = db.department_monthly_revenue()
paint = [r for r in dept_monthly if r['deptId'] == 5]
```

## Files

- `059-paint-seasonal-trend.json` — SceneDocument
- `059-paint-seasonal-trend.md` — this file
