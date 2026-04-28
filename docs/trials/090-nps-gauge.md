# Trial 090: NPS Gauge

**Date:** 2026-03-22
**Goal:** Semi-circular gauge with colored zones and needle indicating score of 72.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

700x500 viewport with one pane.

A 180° gauge arc centered at (0, −0.1) with radius 0.7:
- **Red zone** (0-30, left third) -- Detractors
- **Yellow zone** (30-70, middle) -- Passives
- **Green zone** (70-100, right third) -- Promoters

White needle line points to score 72 (in the green zone). Each arc zone uses 20 center-fan triangles.

Total: 15 unique IDs (1 pane, 2 layers, 4×(buf+geo+di)=12)

---

## Defects Found

### Critical
None.

### Major
None.

### Minor
None.

---

## Spatial Reasoning Analysis

### Done Right
- **Angle mapping.** Score 0→π, score 100→0. Score 72 maps to 0.28π radians (about 50°), correctly in the green zone.
- **Needle length.** 0.6 vs arc radius 0.7 keeps the needle inside the gauge arc.
- **Arc continuity.** Red ends where yellow begins (0.7π), yellow ends where green begins (0.3π).

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Map scores to angles carefully.** The π-to-0 sweep creates a natural left-to-right reading for gauges.
2. **Use separate draw items for colored zones.** Each zone gets its own color through a separate buffer+geo+DI group.
