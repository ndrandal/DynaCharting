# Trial 183: 100 OHLC Candlesticks

**Date:** 2026-03-22
**Goal:** 100 candlesticks with simulated random-walk price data, green up / red down.
**Outcome:** Pending audit.

---

## What Was Built

A 960x640 viewport with 100 candlesticks (instancedCandle@1). Price starts at 100 with Gaussian random walk. Green for close>=open, red for close<open. Half-width=0.35.

## Entity Counts

1 pane, 1 layer, 1 transform, 1 buffer (600 floats), 1 geometry (100 verts), 1 drawItem.

## Data Notes

Random walk: gauss(0,2) per-bar change. High/low add uniform(0,2) beyond open/close. Seed=183.
