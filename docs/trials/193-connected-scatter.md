# Trial 193: Connected Scatter Plot

**Date:** 2026-03-22
**Goal:** 30 time-ordered points connected by a line, showing trajectory over time.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with a lineAA path (29 segments) and 30 overlay points. Line is semi-transparent blue, points are orange with pointSize=5. Data-space transform maps the trajectory to clip.

## Entity Counts

1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

X increments by uniform(0.5,1.5). Y random walk gauss(0,0.5). Seed=193.
