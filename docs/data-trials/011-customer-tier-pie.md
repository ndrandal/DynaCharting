# Data Trial 011: Customer Tier Pie Chart
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** customer_tier_breakdown()
**Goal:** Pie chart showing customer distribution by tier (gold/silver/bronze).
**Outcome:** Three-wedge pie with tier-appropriate colors. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. 3 wedges, 99 total vertices.
Tiers: gold (12.4%), silver (29.4%), bronze (58.2%).
Total: 11 unique IDs.
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
- Customer tiers: gold=62, silver=147, bronze=291.
- Gold tier at 12.4% is the smallest group.
---
## Lessons
1. Custom colors (gold/silver/bronze) provide semantic meaning beyond the default palette.
