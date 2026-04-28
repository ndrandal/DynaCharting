# Trial 046: Weekday-Only Daily Revenue

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 450

**Query:** daily_revenue() filtered to Mon-Fri (weekday() < 5). 450 weekdays out of 630 total days.
**Data Insight:** Average weekday revenue: $1,769. Weekdays show more consistent patterns without weekend volatility.

## Filter Logic

```
daily = db.daily_revenue()
weekday = [r for r in daily if date.fromisoformat(r['date']).weekday() < 5]
```

## Files

- `046-weekday-only-revenue.json` — SceneDocument
- `046-weekday-only-revenue.md` — this file
