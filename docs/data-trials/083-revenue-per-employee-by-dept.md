# Trial 083: Revenue per Employee by Department

**Date:** 2026-03-22
**Goal:** Compute department efficiency: total department revenue / employee headcount. Bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → departmentId → dept_revenue
employees GROUP BY departmentId → dept_headcount
revenue_per_employee = dept_revenue / dept_headcount
```

Two-table join: sale_items x products for revenue, employees for headcount. Cross-table ratio.

## Data Insight

| Department | Revenue | Employees | Rev/Employee |
|------------|---------|-----------|--------------|
| Lumber & Building Materials | $110,695 | 3 | $36,898 |
| Tools & Hardware | $314,325 | 4 | $78,581 |
| Plumbing | $100,060 | 3 | $33,353 |
| Electrical | $128,326 | 2 | $64,163 |
| Paint & Coatings | $122,628 | 3 | $40,876 |
| Garden & Outdoor | $180,956 | 4 | $45,239 |
| Home Décor | $72,258 | 2 | $36,129 |
| Seasonal & Holiday | $186,928 | 2 | $93,464 |

Departments with fewer specialized staff can show higher per-employee revenue, revealing
which teams generate the most value per person.

---

## What Was Built

1000x500 viewport, 8 department-colored bars showing revenue-per-employee ratio.
Each bar is a separate DrawItem for per-department palette coloring.

Total: 27 unique IDs.
