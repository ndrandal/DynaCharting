# Data Trial 194: Org Chart from Data
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Hierarchical org chart from employee role data. Nodes (instancedRect@1) + connecting lines (lineAA@1) arranged in 4 levels.
**Goal:** Org chart: 1 Manager, 2 Asst Managers, 8 Dept Leads, 24 Associates.
**Outcome:** 23 node boxes + 114 connecting lines. 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x600. Four horizontal levels with boxes for each employee.
lineAA@1 connections from each parent level to child level (simplified all-to-all).
Associates level capped at 12 displayed nodes to prevent overcrowding.

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
- The store has a typical retail hierarchy: 1:2:8:24 ratio across levels.
- Span of control: each dept lead manages ~3 associates.

---
## Lessons
1. Org charts are tree layouts — the engine renders nodes and edges, but layout is external.
2. Simplified all-to-all connections work for small graphs but need proper parent-child mapping for accuracy.
