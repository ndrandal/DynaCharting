# Trial 125 — Workforce Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: top 15 employee hours (top), shift heatmap (bottom-left), role distribution (bottom-right)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `employee_hours(top_n=15)` — 15 employees as horizontal instancedRect@1
- Pane 2: `shift_heatmap()` — 126 cells as color-banded instancedRect@1
- Pane 3: employee roles — 7 roles as instancedRect@1

## Insight
Shift heatmap reveals staffing patterns — darker cells indicate understaffed hours. Role distribution shows workforce composition for scheduling optimization.
