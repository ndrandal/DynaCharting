# Trial 045: Silver vs Bronze Tier — Monthly Revenue Comparison

**Date:** 2026-03-22
**Chart Type:** Two overlaid lineAA@1 lines
**Pipeline:** see below
**Data Points:** 42

**Query:** Filter sales by customer tier (silver vs bronze). 147 silver, 291 bronze customers across 21 months.
**Data Insight:** Silver total: $173,569, Bronze total: $214,281. Bronze tier outspends by $40,712.

## Filter Logic

```
silver_ids = {c['id'] for c in db.customers if c['tier'] == 'silver'}
bronze_ids = {c['id'] for c in db.customers if c['tier'] == 'bronze'}
Accumulate monthly revenue per tier
```

## Files

- `045-silver-vs-bronze-bars.json` — SceneDocument
- `045-silver-vs-bronze-bars.md` — this file
