# Data Trial 035: Expense Type Pie
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Custom aggregation — expenses grouped by account type
**Goal:** Pie chart of expense distribution by account type.
**Outcome:** Expense category proportions clear. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. 6 wedges, 129 total vertices.
Types: Payroll ($2,402,429), Occupancy ($526,840), OpEx ($89,913), Facilities ($63,415), Financial ($25,068), Loss ($3,986).
Total: 20 unique IDs.
---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.
---
## Data Insights
- Payroll is the largest expense category at $2,402,429 (77.2%).
- 6 account types.
---
## Lessons
1. Grouping by account type requires joining expenses → accounts via accountId.
