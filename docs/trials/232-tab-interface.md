# Trial 232: Tab Interface

**Date:** 2026-03-22
**Goal:** 4 tabs at top with one active (taller, lighter). Content area below. Tab separators. Tests UI tab pattern with instancedRect@1.
**Outcome:** 4 tabs (1 active + 3 inactive) with content area. 15 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Inactive tabs | instancedRect@1 | 3 |
| 105 | 10 | Active tab | instancedRect@1 | 1 |
| 108 | 10 | Content area | instancedRect@1 | 1 |
| 111 | 11 | Tab separators | lineAA@1 | 3 |

Total: 15 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Active tab is taller and lighter than inactive tabs.
- Content area sits below tab row.
- Tab separators delineate boundaries.

### Done Wrong

Nothing.
