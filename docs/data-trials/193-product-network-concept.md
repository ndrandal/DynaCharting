# Data Trial 193: Product Network Concept
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Network/graph visualization — nodes (products) connected by edges (co-purchase frequency). Requires circular layout computation and graph representation with basic primitives.
**Goal:** Top 10 co-purchased product pairs as a network graph.
**Outcome:** 13 nodes (points@1) + 10 edges (lineAA@1). Max co-purchase: 20 times. 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 800x800 (square). Circular layout with products as orange nodes and translucent edges.
Products placed at equal angles around a circle of radius 0.6.
Labels pushed outward from each node.

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
- Top pair co-purchased 20 times across 12,338 sales.
- Network reveals product affinity clusters — useful for cross-selling.

---
## Lessons
1. Network graphs can be approximated with points@1 (nodes) + lineAA@1 (edges) on circular layout.
2. The engine is not a graph layout engine — positions must be computed externally.
