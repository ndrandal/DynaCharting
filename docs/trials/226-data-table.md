# Trial 226: Data Table

**Date:** 2026-03-22
**Goal:** 5 data rows x 4 columns with header row. Alternating row colors and column/row separators. Tests tabular UI layout.
**Outcome:** 6 rows x 4 columns with header and stripe pattern. 15 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Header cells | instancedRect@1 | 4 |
| 105 | 10 | Even row cells | instancedRect@1 | 8 |
| 108 | 10 | Odd row cells | instancedRect@1 | 12 |
| 111 | 11 | Separators | lineAA@1 | 12 |

Total: 15 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Header row visually distinct with darker background.
- Alternating row stripes improve readability.
- Column widths vary to simulate real data layout.

### Done Wrong

Nothing.
