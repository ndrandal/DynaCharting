# Trial 079: Department Leads — Average Weekly Hours Comparison

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 8

**Query:** Filter employees with role=='Department Lead'. 8 leads.
**Data Insight:** Avg across leads: 31.8 h/wk. Top: Diego R. at 32.6h. Lead workloads vary by department demands.

## Filter Logic

```
dept_leads = [e for e in db.employees if e['role'] == 'Department Lead']
Join with employee_hours() for avg weekly
```

## Files

- `079-dept-leads-weekly-hours.json` — SceneDocument
- `079-dept-leads-weekly-hours.md` — this file
