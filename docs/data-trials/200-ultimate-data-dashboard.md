# Data Trial 200: Ultimate Data Dashboard
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** The grand finale — 4-pane comprehensive dashboard combining revenue trend, department breakdown, product scatter, and customer tier analysis. Tests the engine's ability to compose multiple visualization types in one scene.
**Goal:** Multi-pane dashboard using ALL major data relationships with last-6-months filter.
**Outcome:** 4 panes, ~14 DrawItems, 53 unique IDs. Revenue trend (area+line), department donut (8 wedges), product scatter (80 points), customer tier donut (3 wedges). Zero defects.

---
## What Was Built

Viewport 1200x900. 2x2 pane grid:

| Position | Content | Pipeline |
|----------|---------|----------|
| Top-left | Revenue trend (area + line) | triSolid@1 + lineAA@1 |
| Top-right | Department revenue donut (8 wedges) | triSolid@1 |
| Bottom-left | Product price vs volume (80 points) | points@1 |
| Bottom-right | Customer tier revenue donut (3 wedges) | triSolid@1 |

Each pane has its own clear color, layers, and (where needed) transforms.
Last 6 months filter applied to revenue trend; other panels use full dataset.

This dashboard exercises:
- Multiple panes with independent clipping
- Area fill + line overlay
- Donut charts with per-wedge coloring
- Scatter plots with transforms
- Text overlay spanning all panes

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
- Last 6 months revenue: $375,844
- Tools & Hardware dominates the department donut
- Product scatter shows classic price-volume tradeoff
- Gold tier customers contribute disproportionate revenue

---
## Lessons
1. Multi-pane dashboards are the ultimate composition test — every primitive type in one scene.
2. The declarative SceneDocument format handles complex dashboards cleanly.
3. ID allocation across 4 panes, 5+ layers, and ~14 DrawItems requires careful planning.
