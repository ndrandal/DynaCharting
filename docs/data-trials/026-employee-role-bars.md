# Data Trial 026: Employee Count by Role
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** Custom aggregation from db.employees
**Goal:** Bar chart counting employees by role.
**Outcome:** Role distribution visible. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 7 bars for 7 roles.
Roles: Sales Associate(15), Department Lead(8), Cashier(4), Stock Clerk(3), Assistant Manager(2), Customer Service(2), Store Manager(1).
Total: 6 unique IDs.
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
- Sales Associate is the most common role with 15 employees.
- 7 distinct roles in the organization.
---
## Lessons
1. Custom aggregations (Counter) fill gaps where the adapter has no built-in query.
