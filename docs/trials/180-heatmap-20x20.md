# Trial 180: 20x20 Heatmap Grid

**Date:** 2026-03-22
**Goal:** 400-cell heatmap with viridis-like color scale based on sin(x)*cos(y).
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 400 rectangles (20x20 grid) rendered as triGradient@1. Each cell colored per a viridis approximation of sin(x*0.4)*cos(y*0.3) mapped to [0,1]. Each rect = 2 triangles = 6 vertices in pos2_color4 format.

## Entity Counts

1 pane, 1 layer, 0 transforms, 1 buffer (400*6*6=14400 floats), 1 geometry (2400 verts), 1 drawItem.

## Data Notes

Color value = (sin(ix*0.4)*cos(iy*0.3)+1)/2 through a viridis approximation.
