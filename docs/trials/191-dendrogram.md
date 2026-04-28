# Trial 191: Binary Dendrogram

**Date:** 2026-03-22
**Goal:** Binary tree with horizontal merge links and 15 leaf nodes.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with a dendrogram: lineAA@1 links connecting merged clusters, and 15 leaf node points at the bottom. Adjacent pairs merge at increasing heights. Transform maps data space to clip.

## Entity Counts

1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

15 leaves, merge adjacent pairs up the tree. Seed=191.
