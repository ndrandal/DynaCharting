# Trial 056: Employees Hired in 2024 — Total Hours Worked

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 5

**Query:** Filter employees where hireDate starts with '2024'. 5 employees hired in 2024.
**Data Insight:** Top performer: Terri B. with 2946 hours. New hires show varied ramp-up speeds.

## Filter Logic

```
hired_2024 = [e for e in db.employees if e['hireDate'].startswith('2024')]
Join with employee_hours() for total hours
```

## Files

- `056-new-employees-2024.json` — SceneDocument
- `056-new-employees-2024.md` — this file
