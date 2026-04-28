# Data Trial 184: Waterfall Decomposition
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Waterfall chart decomposing revenue change from first to last month into price effect, volume effect, and mix effect. Floating bars with connectors.
**Goal:** Revenue change decomposed into contributing factors.
**Outcome:** 5 waterfall bars (start, 3 effects, end) with dashed connectors. Delta: $+886. 16 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x600. Waterfall with:
- Blue totals: start ($62,853) and end ($63,739)
- Green/red floating bars: price effect ($+7,885), volume ($-6,222), mix ($-777)
- Dashed connectors between bar tops

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
- Price effect: $+7,885 (change in average ticket * base volume)
- Volume effect: $-6,222 (change in transaction count * base ticket)
- Mix effect: $-777 (interaction/remainder)

---
## Lessons
1. Waterfall charts are floating bars — each bar's baseline is the previous bar's top.
2. Three separate DrawItems (total/positive/negative) provide distinct coloring.
