# Trial 121 — Executive Summary Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes (top-left: monthly revenue line, top-right: dept revenue bars, bottom: monthly profit line)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `monthly_revenue()` — 21 months as lineAA@1
- Pane 2: `department_revenue()` — 8 departments as instancedRect@1
- Pane 3: `monthly_profit()` — 22 months as lineAA@1

## Insight
Combines top-line revenue trend, departmental breakdown, and profitability into a single executive view. The bottom pane shows profit = revenue - expenses, revealing months where high revenue didn't translate to high profit.

## ID Allocation
- Panes: 1, 2, 3 | Layers: 10, 20, 30 | Transforms: 50, 51, 52
- Buffers: 100, 110, 120 | Geometries: 101, 111, 121 | DrawItems: 102, 112, 122
