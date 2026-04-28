# Data Trial 161: Empty Filter
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** What happens when a data filter returns zero results? The engine must handle an empty scene gracefully.
**Goal:** Filter products with unitPrice > $5,000 (none exist — max is $899.99). Show empty pane with informative message.
**Outcome:** Valid scene with 0 DrawItems, 0 buffers, 0 geometries. Background clears correctly. 2 unique IDs. Zero defects.

---
## What Was Built

Viewport 800x500. Single pane with dark background. No buffers, no geometries, no draw items.
The query `unitPrice > 5000` matched 0 of 150 products (max price is $899.99).

Text overlay displays "No products found" message. The engine renders only the pane clear color.

This tests the graceful handling of zero-data scenarios: no geometry, no draw calls, just a background.

---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---
## Data Insights
- The highest product price is $899.99 — the store has no luxury items above $1,000.
- Empty data is a common real-world scenario (date filters with no activity, search with no matches).

---
## Lessons
1. A valid SceneDocument can have zero buffers and zero draw items — the engine handles this gracefully with just pane clear color.
2. Text overlay is essential for communicating "no data" states to users — an empty pane with no explanation is confusing.
