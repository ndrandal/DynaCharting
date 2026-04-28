# Data Trial 172: Extreme Tall
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** 200x2000 viewport — extreme vertical aspect ratio (1:10). Tests horizontal bars in a narrow but tall viewport.
**Goal:** Top 30 products as horizontal bars in a tall, narrow viewport.
**Outcome:** 30 horizontal instancedRect@1 bars. 6 unique IDs. Zero defects.

---
## What Was Built

Viewport 200x2000. Each bar spans the full width, 30 bars stacked vertically.
At 200px wide, each bar has ~60px of useful length — tight but readable.

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
- Top product: Cordless Drill/Driver 20V Kit ($77,152).
- 30th product: Deck Screws #8 5lb Box ($11,388).

---
## Lessons
1. Extreme tall viewports work correctly with horizontal bars.
2. 200px width is the practical minimum for readable horizontal bar charts.
