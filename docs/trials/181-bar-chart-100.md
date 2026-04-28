# Trial 181: 100-Bar Chart with Damped Sine Heights

**Date:** 2026-03-22
**Goal:** 100 vertical bars with alternating blue/cyan, heights following a damped sine wave.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 100 bars (50 blue, 50 cyan). Heights follow h = 0.5 + 0.5*exp(-i*0.02)*sin(i*0.15). Transform maps x=[0,100], y=[0,1] to clip.

## Entity Counts

1 pane, 1 layer, 1 transform, 2 buffers, 2 geometries, 2 drawItems.

## Data Notes

Damped sine: exponential decay envelope modulating a sine wave. 50 bars each color.
