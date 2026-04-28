# Trial 076: One-Time Customers Grouped by Tier

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 bar chart
**Pipeline:** see below
**Data Points:** 3

**Query:** Count sales per customer, filter to count == 1. 4 one-time customers.
**Data Insight:** Gold: 0, Silver: 0, Bronze: 4. Bronze dominates single-purchase behavior — potential loyalty program targets.

## Filter Logic

```
one_timers = {cid for cid, cnt in cust_counts.items() if cnt == 1}
tier_counts = Counter(tier for one-timer customers)
```

## Files

- `076-one-time-customers.json` — SceneDocument
- `076-one-time-customers.md` — this file
