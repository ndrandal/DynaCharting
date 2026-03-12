# Trial 011: Waterfall Chart

**Date:** 2026-03-12
**Goal:** Revenue bridge waterfall chart with floating bars at cumulative running totals, dashed connector lines, and a zero baseline. First trial with bars positioned relative to prior cumulative sums rather than origin.
**Outcome:** Textbook waterfall chart. All 10 bar positions, 9 connector levels, and the running total are exact. Zero major defects.

---

## What Was Built

A 1100×700 viewport with a single pane:

**Waterfall Chart (1078×672px):**
- **10 bars** across 3 `instancedRect@1` DrawItems:
  - Blue totals (2 bars): Q3 Revenue y=[0, 850], Q4 Revenue y=[0, 930]
  - Green positives (4 bars): +120, +65, +40, +30 — floating above the running total
  - Red negatives (4 bars): -95, -45, -20, -15 — hanging below the previous total
- **9 dashed connector lines** (`lineAA@1`, dashLength 6, gapLength 4, gray alpha 0.6): horizontal segments at the running total level between consecutive bars.
- **Zero baseline** (`lineAA@1`, solid gray alpha 0.5): from x=-0.3 to x=10.3 at y=0.
- Bars centered at x = 0.5, 1.5, ..., 9.5 with width 0.7 (x±0.35).
- Viewport: [-0.3, 10.3] × [-50, 1150], static.

Running total: 850 → 970 → 1035 → 1075 → 980 → 935 → 915 → 900 → 930.

Total resources: 1 pane, 3 layers, 1 transform, 5 buffers, 5 geometries, 5 drawItems, 1 viewport = 20 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Format mismatch on lineAA@1 drawItems.** Connectors (geom 203) and baseline (geom 204) declare `rect4` format but use `lineAA@1` which expects `pos2_clip`. Same issue as trial 009. Engine renders correctly regardless.

2. **Small-magnitude bars are barely visible.** The -15 bar (FX Impact) is 15/1200 of the y-range = ~8.4px tall. The -20 bar (Refunds) is ~11.2px. Both are visible but thin. A narrower y-range (e.g., [800, 1100] instead of [-50, 1150]) would make the waterfall detail more prominent, though it would crop the total bars.

3. **Text labels invisible in PNG capture.** Known limitation. Without labels, the bars are identified only by color (green=positive, red=negative, blue=total). The change amounts and running totals are absent.

---

## Spatial Reasoning Analysis

### Done Right

- **Running total is exact.** 850 +120 +65 +40 -95 -45 -20 -15 +30 = 930. Q4 bar top = 930. All intermediate totals verified.

- **All bar positions are correct.** Every green bar starts at the previous running total and ends at the new total. Every red bar spans from the new (lower) total to the previous (higher) total. Both blue totals anchor at y=0. All 10 bars verified:

  | Bar | Type | Y range | Delta | Running Total |
  |-----|------|---------|-------|---------------|
  | 0 | blue | 0→850 | start | 850 |
  | 1 | green | 850→970 | +120 | 970 |
  | 2 | green | 970→1035 | +65 | 1035 |
  | 3 | green | 1035→1075 | +40 | 1075 |
  | 4 | red | 980→1075 | -95 | 980 |
  | 5 | red | 935→980 | -45 | 935 |
  | 6 | red | 915→935 | -20 | 915 |
  | 7 | red | 900→915 | -15 | 900 |
  | 8 | green | 900→930 | +30 | 930 |
  | 9 | blue | 0→930 | end | 930 |

- **All 9 connector levels are exact.** Each connector is a horizontal segment at the running total between the bar that establishes it and the next bar. Verified all 9: 850, 970, 1035, 1075, 980, 935, 915, 900, 930 ✓.

- **Connector transitions handle direction changes correctly.** Connector 3 (1075) connects bar 3's top to bar 4's top (both at the peak). Connector 7 (900) connects bar 7's bottom to bar 8's bottom (both at the trough). The connectors always link at the running total, regardless of whether the next bar goes up or down.

- **Transform is mathematically exact.** sx = 1.96/10.6 = 0.184905660377, sy = 1.92/1200 = 0.0016, tx = -0.924528301887, ty = -0.88. All verified to 12+ significant figures.

- **Layer ordering is correct.** Connectors (10) drawn first, baseline (11) second, bars (12) on top. The dashed connectors are visible behind the bar edges.

- **All 20 IDs unique.** Pane 1, layers 10-12, transform 50, buffers 100-104, geometries 200-204, drawItems 300-304.

### Done Wrong

- **Repeated format mismatch for lineAA@1.** This is now the third consecutive trial (009, 010 didn't use lineAA, but 011) where lineAA@1 geometries declare rect4 format. The agent hasn't internalized the lesson from the trial 009 audit.

---

## Lessons for Future Trials

1. **Waterfall charts require careful running-total bookkeeping.** Each bar's y-position depends on all previous bars. The key: maintain a running total variable, update it after each bar, and use it as the starting y for the next bar. Green bars: y1=running, y2=running+delta. Red bars: y1=running+delta (lower), y2=running (higher).

2. **Connector lines bridge the narrative.** Without connectors, a waterfall chart looks like disconnected floating bars. The dashed horizontal lines at each running total level visually link consecutive bars and guide the viewer's eye through the bridge.

3. **The format mismatch pattern must be fixed.** Three trials in a row have lineAA@1 geometries declaring rect4. Future specs should explicitly state: "lineAA@1 geometries MUST use pos2_clip format."

4. **Viewport range tradeoff for waterfall charts.** The y-range must accommodate both the full total bars (anchored at 0) AND the floating incremental bars (at ~850-1075). This compresses the incremental bars to ~25% of the vertical space. An alternative: use a "broken axis" with the 0-based total bars truncated, but this adds complexity.
