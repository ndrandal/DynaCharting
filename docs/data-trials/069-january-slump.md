# Trial 069: January Slump — All January Days Across All Years

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 bar chart
**Pipeline:** see below
**Data Points:** 62

**Query:** daily_revenue() filtered to month == '01'. 62 January days.
**Data Insight:** January avg: $1,391/day vs overall avg: $2,109/day. Below average by 34%. Post-holiday budget tightening visible.

## Filter Logic

```
jan = [r for r in daily if r['date'][5:7] == '01']
```

## Files

- `069-january-slump.json` — SceneDocument
- `069-january-slump.md` — this file
