# Trial 095: Department Lead Contribution

**Date:** 2026-03-22
**Goal:** For each department lead, show their department's total revenue. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
departments.managerId → employees (name lookup)
sale_items JOIN products ON productId → departmentId
GROUP BY departmentId → SUM(lineTotal)
```

Three-table chain: departments -> employees (for names), sale_items -> products (for revenue).

## Data Insight

| Lead | Department | Revenue |
|------|------------|---------|
| Diego Ramirez | Lumber & Building Materials | $110,695 |
| James O'Brien | Tools & Hardware | $314,325 |
| Aisha Patel | Plumbing | $100,060 |
| Tom Kowalski | Electrical | $128,326 |
| Linda Nguyen | Paint & Coatings | $122,628 |
| Robert Garcia | Garden & Outdoor | $180,956 |
| Kenji Tanaka | Home Décor | $72,258 |
| Fatima Al-Hassan | Seasonal & Holiday | $186,928 |

This shows each department lead's accountability scope. The lead overseeing the
highest-revenue department has the most commercial responsibility.

---

## What Was Built

1000x500 viewport, 8 pink horizontal bars.

Total: 6 unique IDs.
