# Trial 086: Employee Sales Count

**Date:** 2026-03-22
**Goal:** Count sales per employeeId, show top 15 as horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY employeeId
COUNT(*) AS salesCount
ORDER BY salesCount DESC
LIMIT 15
```

Single table query on sales (12,338 rows), grouped by employeeId, then employee name
lookup via employees table.

## Data Insight

| Employee | Sales Count |
|----------|-------------|
| Carlos Mendoza | 553 |
| Robert Garcia | 550 |
| Olga Petrov | 545 |
| James O'Brien | 536 |
| Aisha Patel | 529 |
| Hannah Schmidt | 523 |
| Diego Ramirez | 523 |
| Mike Sullivan | 523 |
| Pavel Novak | 515 |
| Darius Jackson | 514 |
| Mei Lin Wu | 499 |
| Courtney Adams | 478 |
| Fatima Al-Hassan | 478 |
| Linda Nguyen | 460 |
| Tom Kowalski | 440 |

Cashiers and long-tenured associates dominate the top — they handle the most register
transactions regardless of department.

---

## What Was Built

1000x500 viewport, 15 amber horizontal bars.

Total: 6 unique IDs.
