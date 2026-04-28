# Trial 050: High-Ticket Sales (>$200) — Count Per Month

**Date:** 2026-03-22
**Chart Type:** instancedRect@1 bar chart
**Pipeline:** see below
**Data Points:** 21

**Query:** Filter sales where total > $200. 1747 qualifying sales out of 12338 total.
**Data Insight:** 1747 high-ticket sales (14.2% of all sales), avg 83.2/month. These premium transactions represent the store's big-project customers.

## Filter Logic

```
high_ticket = [s for s in db.sales if s['total'] > 200]
monthly_count[s['date'][:7]] += 1
```

## Files

- `050-high-ticket-sales.json` — SceneDocument
- `050-high-ticket-sales.md` — this file
