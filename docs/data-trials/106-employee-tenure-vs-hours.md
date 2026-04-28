# Trial 106: Employee Tenure vs Weekly Hours

**Date:** 2026-03-22
**Goal:** For each employee: months since hire vs average weekly hours worked. Scatter.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
employees.hireDate → tenure_months = (2026-03 - hireDate) in months
shifts GROUP BY employeeId → totalHours / weeks → avgWeeklyHours
Plot: (tenure_months, avgWeeklyHours)
```

Two-table join: employees x shifts.

## Data Insight

35 employees plotted. Longer-tenured employees may work more consistent hours
(full-time) while newer hires may be part-time. The scatter reveals whether tenure
correlates with schedule commitment.

---

## What Was Built

800x600 viewport, 35 pink scatter points. X = tenure months, Y = avg weekly hours.

Total: 6 unique IDs.
