# Trial 111: Expense Stacked Monthly

**Date:** 2026-03-22
**Goal:** All expense accounts stacked by month. to_stacked_bars for top 5 accounts.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
expenses GROUP BY (accountId, month) → SUM(amount)
accounts → name lookup
Top 5 accounts by total: Salaries & Wages, Rent, Payroll Taxes, Employee Benefits, Utilities
22 months of data
```

Two-table join: expenses (238) x accounts (15).

## Data Insight

The stacked bars reveal expense seasonality and composition. Payroll is typically the
largest expense category. Months with spikes in other accounts may indicate inventory
buildups or one-time capital expenditures.

---

## What Was Built

1200x500 viewport, 22 stacked bar columns with 5 account segments each.

Total: 18 unique IDs.
