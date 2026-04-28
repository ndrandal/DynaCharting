# Trial 064: Above-Average Employees by Weekly Hours

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 27

**Query:** employee_hours() filtered where avgWeeklyHours > mean (31.0). 27 of 35 employees.
**Data Insight:** Top worker: Sarah Johansson at 41.0 h/wk. These 27 employees consistently exceed the store average.

## Filter Logic

```
mean = sum(avgWeeklyHours) / len(employees) = 31.0
above = [e for e in employee_hours() if avgWeeklyHours > mean]
```

## Files

- `064-above-avg-employees.json` — SceneDocument
- `064-above-avg-employees.md` — this file
