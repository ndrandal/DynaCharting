# Trial 060: Plumbing Dept — Top 10 Products by Units Sold

**Date:** 2026-03-22
**Chart Type:** Horizontal instancedRect@1 bars
**Pipeline:** see below
**Data Points:** 10

**Query:** product_rankings() filtered to deptId==3, sorted by units desc, top 10.
**Data Insight:** Top seller: Teflon Tape 1/2in Roll with 628 units. Plumbing essentials (fittings, pipes) dominate volume.

## Filter Logic

```
plumbing = [r for r in product_rankings() if r['deptId'] == 3]
plumbing.sort(key=lambda r: -r['units'])[:10]
```

## Files

- `060-plumbing-top-sellers.json` — SceneDocument
- `060-plumbing-top-sellers.md` — this file
