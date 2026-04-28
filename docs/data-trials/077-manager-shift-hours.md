# Trial 077: Store Manager (Patricia Chen) — Monthly Shift Hours

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter shifts to employeeId==1 (Patricia Chen). 409 shifts across 21 months.
**Data Insight:** Avg 175 hours/month. Manager workload is consistently high across all seasons.

## Filter Logic

```
manager_shifts = [sh for sh in db.shifts if sh['employeeId'] == 1]
monthly_hours[sh['date'][:7]] += sh['hoursWorked']
```

## Files

- `077-manager-shift-hours.json` — SceneDocument
- `077-manager-shift-hours.md` — this file
