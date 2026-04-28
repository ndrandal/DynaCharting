# Trial 138 — Weekend vs Weekday Dashboard
**Date:** 2026-03-22
**Layout:** 2 panes: left = weekday avg revenue bars, right = weekend avg revenue bars
**Resolution:** 1200x800

## Data Sources
- Left: `dow_distribution()` filtered to Mon-Fri — 5 bars (avg daily revenue)
- Right: `dow_distribution()` filtered to Sat-Sun — 2 bars (avg daily revenue)

## Insight
Same Y scale enables direct comparison between weekday and weekend spending patterns. Weekend days may have higher or lower per-day revenue depending on store traffic patterns.
