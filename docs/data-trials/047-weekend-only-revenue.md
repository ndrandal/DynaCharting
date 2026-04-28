# Trial 047: Weekend-Only Daily Revenue

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 180

**Query:** daily_revenue() filtered to Sat-Sun (weekday() >= 5). 180 weekend days.
**Data Insight:** Average weekend revenue: $2,959. Weekend patterns show DIY project shoppers with different spending behavior.

## Filter Logic

```
daily = db.daily_revenue()
weekend = [r for r in daily if date.fromisoformat(r['date']).weekday() >= 5]
```

## Files

- `047-weekend-only-revenue.json` — SceneDocument
- `047-weekend-only-revenue.md` — this file
