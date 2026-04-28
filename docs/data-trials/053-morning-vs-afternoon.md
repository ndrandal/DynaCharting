# Trial 053: Morning vs Afternoon — Monthly Revenue Comparison

**Date:** 2026-03-22
**Chart Type:** Two overlaid lineAA@1 lines
**Pipeline:** see below
**Data Points:** 42

**Query:** Split sales by time: hour < 12 (morning) vs hour >= 12 (afternoon). 21 months.
**Data Insight:** Morning total: $306,733, Afternoon total: $1,021,938. Afternoon dominates by $715,204. Contractor morning rush vs DIY afternoon shoppers.

## Filter Logic

```
Split sales: hour = int(s['time'].split(':')[0])
hour < 12 -> morning, else -> afternoon
Accumulate monthly revenue per period
```

## Files

- `053-morning-vs-afternoon.json` — SceneDocument
- `053-morning-vs-afternoon.md` — this file
