# Trial 073: High Revenue but Low Margin Products

**Date:** 2026-03-22
**Chart Type:** points@1 scatter plot (revenue x, margin y)
**Pipeline:** see below
**Data Points:** 25

**Query:** Top 30 by revenue intersected with bottom 50% by margin. 25 products qualify.
**Data Insight:** These 25 products drive revenue but eat margin. Consider price increases or supplier renegotiation.

## Filter Logic

```
top_30_rev = top 30 by revenue
bottom_half = bottom 50% by margin
targets = intersection of both sets
```

## Files

- `073-high-rev-low-margin.json` — SceneDocument
- `073-high-rev-low-margin.md` — this file
