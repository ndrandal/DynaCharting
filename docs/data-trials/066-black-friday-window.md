# Trial 066: Black Friday Window (Nov 20-30) — Daily Revenue

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 bar chart
**Pipeline:** see below
**Data Points:** 22

**Query:** daily_revenue() filtered to November 20-30 across all years. 22 days.
**Data Insight:** Peak day: 2025-11-28 at $7,729. Black Friday and surrounding days show elevated spending.

## Filter Logic

```
bf = [r for r in daily if month == '11' and 20 <= day <= 30]
```

## Files

- `066-black-friday-window.json` — SceneDocument
- `066-black-friday-window.md` — this file
