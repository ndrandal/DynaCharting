# Trial 057: Full-Time vs Part-Time — Average Weekly Hours Comparison

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 grouped bars
**Pipeline:** see below
**Data Points:** 2

**Query:** Split employee_hours() by avgWeeklyHours > 30 (full-time) vs <= 30 (part-time). 27 FT, 8 PT employees.
**Data Insight:** Full-time avg: 32.9 h/wk, Part-time avg: 24.5 h/wk. FT employees work 1.3x the hours of PT staff.

## Filter Logic

```
full_time = [r for r in emp_hours if r['avgWeeklyHours'] > 30]
part_time = [r for r in emp_hours if r['avgWeeklyHours'] <= 30]
```

## Files

- `057-full-time-vs-part-time.json` — SceneDocument
- `057-full-time-vs-part-time.md` — this file
