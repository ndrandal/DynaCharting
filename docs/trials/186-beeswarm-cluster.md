# Trial 186: Beeswarm Cluster Plot

**Date:** 2026-03-22
**Goal:** 100 points in a beeswarm/violin shape, denser in the middle, sparse at edges.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 100 points arranged in a beeswarm shape. Y values are Gaussian (sigma=0.3). X jitter width is proportional to the density at each Y, creating a violin-like shape. pointSize=4. Directly in clip space.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (200 floats), 1 geometry (100 verts), 1 drawItem.

## Data Notes

Y: gauss(0, 0.3). X width: exp(-y^2/(2*0.09))*0.4. Seed=186.
