# Trial 120: Revenue per Hour by Employee

**Date:** 2026-03-22
**Goal:** For top 15 employees: total revenue of their sales / total hours worked. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sales GROUP BY employeeId → SUM(total) AS empRevenue
shifts GROUP BY employeeId → SUM(hoursWorked) AS empHours
revPerHour = empRevenue / empHours
WHERE empHours >= 100 (exclude low-hour workers)
ORDER BY revPerHour DESC LIMIT 15
```

Two-table cross-reference: sales (12,338) x shifts (13,751), joined by employeeId.

## Data Insight

| Employee | Revenue | Hours | Rev/Hour |
|----------|---------|-------|----------|
| Robert Garcia | $66,502 | 2,871h | $23/h |
| James O'Brien | $63,414 | 2,903h | $22/h |
| Aisha Patel | $61,275 | 2,908h | $21/h |
| Mike Sullivan | $61,251 | 2,942h | $21/h |
| Raj Mehta | $46,692 | 2,255h | $21/h |
| Olga Petrov | $59,131 | 2,889h | $20/h |
| Pavel Novak | $57,835 | 2,850h | $20/h |
| Diego Ramirez | $57,775 | 2,969h | $19/h |
| Fatima Al-Hassan | $54,967 | 2,853h | $19/h |
| Carlos Mendoza | $56,718 | 2,964h | $19/h |
| Hannah Schmidt | $54,860 | 2,886h | $19/h |
| Jasmine Brown | $42,327 | 2,291h | $18/h |
| Crystal Johnson | $41,016 | 2,276h | $18/h |
| Courtney Adams | $51,650 | 2,907h | $18/h |
| Sam O'Donnell | $33,759 | 1,940h | $17/h |

Revenue per hour is the ultimate employee efficiency metric. It combines sales performance
with time investment, normalizing for full-time vs part-time differences.

---

## What Was Built

1000x500 viewport, 15 cyan bars for revenue-per-hour.

Total: 6 unique IDs.
