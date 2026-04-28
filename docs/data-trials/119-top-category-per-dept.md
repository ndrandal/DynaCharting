# Trial 119: Top Category per Department

**Date:** 2026-03-22
**Goal:** For each department, the single highest-revenue category. Horizontal bars.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
sale_items JOIN products ON productId → (departmentId, category)
GROUP BY (departmentId, category) → SUM(lineTotal)
For each department: MAX by revenue → top category
```

Two-table join with per-group MAX selection.

## Data Insight

| Department | Top Category | Revenue |
|------------|-------------|---------|
| Lumber & Building Materials | sheathing | $27,138 |
| Tools & Hardware | power tools | $177,626 |
| Plumbing | fixtures | $42,681 |
| Electrical | wire | $43,583 |
| Paint & Coatings | interior paint | $37,456 |
| Garden & Outdoor | power equip | $37,630 |
| Home Décor | cabinet hw | $9,560 |
| Seasonal & Holiday | heating | $31,339 |

Each department's identity is defined by its top-selling category. This shows the
revenue concentration within each department's product mix.

---

## What Was Built

1000x500 viewport, 8 department-colored horizontal bars.

Total: 27 unique IDs.
