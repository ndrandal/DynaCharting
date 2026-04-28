# Trial 231: Breadcrumb Trail

**Date:** 2026-03-22
**Goal:** 4 breadcrumb segments as arrow-shaped pentagons (rectangle + triangle point). Last segment highlighted as active. Tests triSolid@1 polygon composition.
**Outcome:** 4 arrow segments, 3 normal + 1 active (green). 8 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Normal breadcrumbs | triSolid@1 | 9 tris (3 segments) |
| 105 | 10 | Active breadcrumb | triSolid@1 | 3 tris |

Each segment = 2 body triangles + 1 arrow triangle = 3 triangles.

Total: 8 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Arrow shapes point right, creating visual flow.
- Active last segment differentiated by green color.

### Done Wrong

Nothing.
