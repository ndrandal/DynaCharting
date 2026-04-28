# Trial 144 — Employee Performance Dashboard
**Date:** 2026-03-22
**Layout:** 3 panes: hours ranking (top-left), revenue per hour (top-right), shift heatmap (bottom)
**Resolution:** 1200x800

## Data Sources
- Pane 1: `employee_hours(top_n=15)` — 15 employees as instancedRect@1
- Pane 2: computed revenue/hour — 15 employees as instancedRect@1
- Pane 3: `shift_heatmap()` — 126 cells as banded instancedRect@1

## Insight
Hours worked alone doesn't tell the story — revenue per hour reveals true productivity. The heatmap shows when the store is staffed vs understaffed.
