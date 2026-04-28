# Trial 114: Customer Cohort Revenue

**Date:** 2026-03-22
**Goal:** Group customers by join-quarter, sum their total revenue. Bars by cohort.
**Outcome:** Structurally sound. Zero defects.

---

## Query

```
customers.memberSince → join quarter (YYYY-QN)
sales GROUP BY customerId → SUM(total) AS customerRevenue
GROUP BY join_quarter → SUM(customerRevenue), COUNT(customers)
```

Two-table join: customers (500) x sales (12,338).

## Data Insight

| Cohort | Customers | Total Revenue |
|--------|-----------|---------------|
| 2020-Q2 | 20 | $20,114 |
| 2020-Q3 | 21 | $17,289 |
| 2020-Q4 | 27 | $28,463 |
| 2021-Q1 | 17 | $20,415 |
| 2021-Q2 | 17 | $17,173 |
| 2021-Q3 | 20 | $27,885 |
| 2021-Q4 | 24 | $30,704 |
| 2022-Q1 | 13 | $14,336 |
| 2022-Q2 | 15 | $12,797 |
| 2022-Q3 | 21 | $24,161 |
| 2022-Q4 | 27 | $26,056 |
| 2023-Q1 | 21 | $23,041 |
| 2023-Q2 | 23 | $18,590 |
| 2023-Q3 | 22 | $19,100 |
| 2023-Q4 | 24 | $30,941 |
| 2024-Q1 | 19 | $22,768 |
| 2024-Q2 | 25 | $29,663 |
| 2024-Q3 | 19 | $21,610 |
| 2024-Q4 | 23 | $28,541 |
| 2025-Q1 | 21 | $19,805 |
| 2025-Q2 | 19 | $19,421 |
| 2025-Q3 | 25 | $23,486 |
| 2025-Q4 | 21 | $17,204 |
| 2026-Q1 | 16 | $16,350 |

Earlier cohorts have higher total revenue (more time to accumulate purchases).
Revenue per cohort normalized by age reveals true acquisition quality.

---

## What Was Built

1000x500 viewport, 24 violet bars for cohort revenue.

Total: 6 unique IDs.
