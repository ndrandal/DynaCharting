# Trial 074: Top 20 Customers by Purchase Frequency

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 20

**Query:** Count sales per customerId, top 20. 500 unique customers total.
**Data Insight:** Most frequent: Jeffrey J. with 33 visits. Top 20 account for 530 total visits.

## Filter Logic

```
cust_counts = Counter(s['customerId'] for s in db.sales if customerId is not None)
top_20 = cust_counts.most_common(20)
```

## Files

- `074-customer-frequency-top20.json` — SceneDocument
- `074-customer-frequency-top20.md` — this file
