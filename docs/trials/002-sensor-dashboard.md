# Trial 002: Sensor Dashboard

**Date:** 2026-03-12
**Goal:** 3-pane IoT sensor dashboard with temperature line, humidity bars, and system health indicators.
**Outcome:** Structurally correct, visually legible. Several layout and styling issues.

---

## What Was Built

A 1000x700 viewport with three vertically stacked panes:

1. **Temperature pane** (top ~45%) — Cyan lineAA trace showing 24-hour temperature curve (15-28.5 C), a green translucent safe-zone band (18-25 C), and three red anomaly point markers at hours where temperature exits the safe zone.

2. **Humidity pane** (middle ~30%) — Blue semi-transparent instancedRect bars for 24-hour humidity readings (48-82%), with an orange dashed threshold line at 60%.

3. **System Health pane** (bottom ~24%) — Five horizontal bars in pre-computed clip space: green for Cooling (92%), Airflow (88%), Power (95%); yellow for Filtration (78%); red for Pressure (45%). Rounded corners specified.

Two linked viewports (`"time"` link group) enable synchronized X-axis pan/zoom across temperature and humidity panes. Humidity viewport locks Y-axis pan/zoom.

Text overlay defines pane titles, Y-axis tick labels, and health bar labels — visible only in browser live-viewer (not in --png capture).

---

## Defects Found

### Critical

None. The chart renders without errors, produces valid output (5.6MB frame), and all data is correctly represented.

### Major

1. **Safe zone band barely visible.** The green band uses alpha 0.15, which against the dark background (`[0.102, 0.102, 0.180]`) is nearly invisible. The band is technically present and correctly positioned (y=18 to y=25 in data space), but a viewer would struggle to notice it. Alpha 0.25-0.35 would be appropriate for a functional safe-zone indicator.

2. **Humidity bars float above pane bottom.** The bar baseline is y=40 but the viewport yMin is 35, leaving a visible gap between the bottom of the bars and the pane edge. This wastes ~9% of the pane's vertical space on empty area below the data. The baseline should match the viewport yMin, or the viewport yMin should match the baseline.

3. **Health bars have no transformId.** DrawItems 117, 120, 123 omit `transformId`, meaning they render with identity transform. This works because the vertex data is pre-computed in clip space — but it means the health pane has no viewport and cannot participate in any interactive behavior. This is intentional for static bars, but inconsistent with the other panes. Not a rendering bug, but a design gap.

### Minor

1. **Text labels invisible in PNG capture.** The `--png` mode bypasses the TEXT protocol, so all pane titles, axis labels, and health bar labels are absent from the screenshot. This is a known engine limitation (text overlay is browser-rendered), not an agent error. However, without labels, the visualization loses significant context.

2. **Anomaly point at hour 3 is debatable.** Hour 3 (15.5 C) is correctly outside the safe zone (18-25 C), but the anomaly at hour 13 (27.8 C) and hour 14 (28.5 C) are also outside. The selection is logically correct. However, hours 0-4 (all below 18 C) and hours 12-15 (above 25 C) are all outside the safe zone — marking only 3 of ~10 anomalous hours is inconsistent. This is a spec interpretation issue rather than a code bug.

3. **Threshold line extends beyond data range.** The humidity threshold line runs from x=-1 to x=24, which matches the viewport range but extends slightly past the first and last bars (which span -0.35 to 23.35). Visually acceptable but the overshoot is ~0.65 units on each side.

4. **Health bar vertical ordering doesn't match intuition.** From top to bottom: Cooling, Filtration, Pressure, Airflow, Power. The text labels (visible only in browser) list the same order, but grouping all green bars together (117) then yellow (120) then red (123) as separate draw items means the visual order is determined by clip-space Y coordinates across three draw calls, not by a single sorted list. This works but is fragile — reordering would require coordinating across multiple buffers.

---

## Spatial Reasoning Analysis

### Done Right

- **Pane region math.** Three panes with small gaps between them: pane 1 clipY [0.1, 1.0], pane 2 [-0.5086, 0.0914], pane 3 [-1.0, -0.5171]. The gaps (~0.86% of viewport height each, ~6px) are intentional and provide clean pane separation. No overlap.

- **Viewport-to-transform pipeline.** Temperature viewport maps data range [-1,24] x [14,30] into pane 1's clip region. Humidity viewport maps [-1,24] x [35,90] into pane 2's clip region. The sx/sy/tx/ty math is handled by the engine's Viewport class — the agent correctly declared data ranges and let the engine compute transforms.

- **Instanced rect data format.** All rect4 data (humidity bars, threshold line, health bars) uses correct (x0, y0, x1, y1) format with proper vertex counts (data.length / 4 = vertexCount).

- **LineAA segment format.** Temperature line uses 23 segments encoded as rect4 (x0,y0,x1,y1 per segment), correctly sharing endpoints between consecutive segments (segment N ends where segment N+1 begins).

- **Health bar clip-space positioning.** Pre-computed clip coordinates place 5 horizontal bars within pane 3's region [-1.0, -0.5171] with consistent bar height (~0.06 clip units) and spacing. The bars are correctly positioned and don't overflow the pane.

### Done Wrong

- **Safe zone band vertex format mismatch.** The band uses `pos2_clip` format (2 floats per vertex = x,y) and `triSolid@1` pipeline with 6 vertices forming 2 triangles. The data is in data space (x: -1 to 24, y: 18 to 25) but `pos2_clip` implies clip-space coordinates. This works only because the band's geometry goes through transform 50 (the temperature viewport transform), which maps these data-space values to clip space. The format name is misleading — the data is actually in data space, not clip space. This is not a rendering bug (the transform handles the mapping), but the format semantics are confusing.

- **Humidity bar baseline/viewport disconnect.** Setting baseline at 40 with viewport yMin=35 shows a lack of end-to-end spatial verification. The agent should have verified that the lowest visible Y in the viewport matches the bar baseline, or explicitly chosen the gap as a design decision (which it didn't — the comment says "baseline at 40" without justification for the 5-unit gap).

---

## Lessons for Future Trials

1. **Verify alpha values against background color.** An alpha of 0.15 on a dark background produces almost no visible change. For overlays on dark backgrounds, start at alpha >= 0.25. Test mentally: `0.15 * green + 0.85 * dark ≈ dark`.

2. **Align bar baselines with viewport bounds.** If bars start at y=40, set viewport yMin=40 (or very close). Wasted viewport space below data reduces the effective resolution of the visualization.

3. **Mark all anomalies or none.** If the spec says "anomaly points outside safe zone," marking 3 of 10 qualifying data points is inconsistent. Either mark all points outside the zone or define a different anomaly criterion (e.g., "peak outliers only").

4. **Text labels are invisible in --png mode.** Any trial that relies on text labels for context will look incomplete in the PNG audit. Future specs should either account for this or add visual elements (colored borders, separators) that convey pane identity without text.

5. **Pre-computed clip-space data is fragile.** The health bars use hardcoded clip coordinates, which means any change to pane 3's region breaks them. For static non-interactive panes this is acceptable, but document the dependency. Better: use a dedicated transform for health pane and express bar data in a normalized data space (0-100%).
