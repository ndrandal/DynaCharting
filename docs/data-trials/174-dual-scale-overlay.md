# Data Trial 174: Dual Scale Overlay
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Two metrics with vastly different scales (revenue ~$60K vs count ~600) on the same pane. Requires TWO different transforms applied to different DrawItems in the same pane.
**Goal:** Revenue (dollars, left axis) and transaction count (right axis) overlaid.
**Outcome:** Two lineAA@1 lines with independent transforms mapping to the same clip region. 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 1000x600. Single pane, single layer, two DrawItems with different transforms.
Blue line: monthly revenue (transform 50 maps $-range to clip).
Orange line: transaction count (transform 51 maps count-range to clip).

Both lines fill the same clip region [-0.85, 0.85] despite different data scales.

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
- Revenue and transaction count move together — correlation suggests consistent average ticket size.
- The dual-scale overlay reveals this relationship without needing a correlation calculation.

---
## Lessons
1. Dual-axis charts are achieved by giving each DrawItem its own transform — the engine supports multiple transforms per pane.
2. Color-coding and labels are critical since there are no visible axis numbers.
