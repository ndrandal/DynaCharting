# Trial 194: Multi-Slope Chart

**Date:** 2026-03-22
**Goal:** 10 slope lines connecting two columns of dots, showing change between periods A and B.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 10 colored slope lines (lineAA@1) connecting points at x=-0.7 (period A) and x=0.7 (period B). White dots at both endpoints. Each line has a distinct color.

## Entity Counts

1 pane, 2 layers, 0 transforms, 11 buffers, 11 geometries, 11 drawItems (10 lines + 1 points).

## Data Notes

A values: uniform(0.1, 0.9). B = A + gauss(0, 0.2). Seed=194.
