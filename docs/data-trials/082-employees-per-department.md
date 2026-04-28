# Trial 082: Employees per Department

**Date:** 2026-03-22
**Goal:** Count employees by departmentId, show as department-colored bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
employees GROUP BY departmentId
COUNT(*) AS headcount
(excluding employees with departmentId = null — managers, cashiers, stock clerks)
```

Join: employees (35 rows), filter to those with non-null departmentId.
Aggregation: count per department.

## Data Insight

| Department | Headcount |
|------------|-----------|
| Lumber & Building Materials | 3 |
| Tools & Hardware | 4 |
| Plumbing | 3 |
| Electrical | 2 |
| Paint & Coatings | 3 |
| Garden & Outdoor | 4 |
| Home Décor | 2 |
| Seasonal & Holiday | 2 |

Most departments have 2-4 people. Garden & Outdoor and Tools & Hardware are the most
staffed departments, reflecting higher customer traffic in those areas.

---

## What Was Built

1000x500 viewport, single pane, 8 bars each with its department's palette color.
Each bar is a separate DrawItem for per-department coloring.

Total: 27 unique IDs.
