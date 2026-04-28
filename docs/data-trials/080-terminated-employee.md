# Trial 080: Terminated Employee: Sam O'Donnell — Monthly Hours Until Departure

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 14

**Query:** Filter shifts to employeeId==26 (Sam O'Donnell, terminated 2025-08-15). 276 shifts across 14 months.
**Data Insight:** Total hours: 1941, avg 139/month. Final months may show declining hours as departure approached.

## Filter Logic

```
shifts = [sh for sh in db.shifts if sh['employeeId'] == 26]
monthly_hours[sh['date'][:7]] += sh['hoursWorked']
Terminated: 2025-08-15
```

## Files

- `080-terminated-employee.json` — SceneDocument
- `080-terminated-employee.md` — this file
