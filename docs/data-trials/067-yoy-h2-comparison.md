# Trial 067: Year-over-Year H2 Comparison (Jul-Dec 2024 vs 2025)

**Date:** 2026-03-22
**Chart Type:** Two overlaid lineAA@1 (indexed 0-5 for month offset)
**Pipeline:** see below
**Data Points:** 12

**Query:** Aggregate daily_revenue() into monthly for Jul-Dec each year. 2024: 6 months, 2025: 6 months.
**Data Insight:** H2 2024 total: $374,564, H2 2025 total: $416,163. Growth of 11.1% YoY.

## Filter Logic

```
h2_2024 = daily filtered Jul-Dec 2024
h2_2025 = daily filtered Jul-Dec 2025
Aggregate to monthly, overlay on same 0-5 index axis
```

## Files

- `067-yoy-h2-comparison.json` — SceneDocument
- `067-yoy-h2-comparison.md` — this file
