# Trial 070: Credit Card Share of Monthly Revenue (%)

**Date:** 2026-03-22
**Chart Type:** lineAA@1 trend line
**Pipeline:** see below
**Data Points:** 21

**Query:** Compute credit_card sales / total sales per month. 21 months.
**Data Insight:** Average CC share: 51.0%. Trend reveals whether digital payment adoption is growing.

## Filter Logic

```
cc_monthly[mk] += s['total'] for paymentMethod == 'credit_card'
pct = cc_monthly / total_monthly * 100
```

## Files

- `070-credit-card-pct-monthly.json` — SceneDocument
- `070-credit-card-pct-monthly.md` — this file
