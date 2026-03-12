# Trial 001: Market Command Center Dashboard

**Date:** 2026-03-12
**Goal:** Build a 5-pane "Market Command Center" dashboard using the DynaCharting engine's JSON command system. Serve it on localhost via the live-viewer bridge.
**Outcome:** Functional but visually rough. Two critical bugs found during development.

## What Was Built

A 1200x800 dashboard with 5 panes:
- **Pane 1** (top-left, 70% height): 120 OHLC candles + SMA(20) line + area fill + dashed level lines + grid
- **Pane 2** (bottom-left, 30% height): Volume bars colored by direction
- **Pane 3** (top-right): 8-sector donut chart with AA
- **Pane 4** (mid-right): 8 horizontal performance bars with rounded corners
- **Pane 5** (bottom-right): 6 sparkline mini-charts in 2x3 grid

Interactive pan/zoom on candle+volume panes with linked X-axis.

File: `core/demos/dashboard_server.cpp` (~1000 lines)

---

## Bug 1: Unified ID Namespace Collision (CRITICAL)

### Symptom
Candle pane and volume pane rendered completely empty (only background color). Right-side panes (pie, bars, sparklines) rendered correctly.

### Root Cause
The engine's `ResourceRegistry` enforces a **single unified namespace** for ALL resource IDs. Every pane, layer, drawItem, buffer, geometry, and transform must have a **globally unique** ID. This was not obvious and is easy to forget.

The original code assigned:
- Sparkline layers: IDs **50**, **51**
- Candle/volume transforms: IDs **50**, **51**

Layers were created first. When `createTransform` was called with id=50 and id=51, the ResourceRegistry rejected them silently (ID already taken by the layers). The transforms never existed. DrawItems bound to transform 60/61 (after fix) rendered with identity transform, placing data-space coordinates (timestamps ~1.7 billion, prices ~42000) directly into clip space [-1,1] — completely off-screen.

### Why It Was Hard to Find
- `cmd()` helper did not check `CmdResult.ok` — failures were silent
- `stderr` was redirected to `/dev/null` — no error output visible
- Right-side panes (no transforms) rendered fine, suggesting the engine was working
- The symptoms looked like a rendering/GPU issue, not a command issue

### Fix
Changed sparkline layer IDs from 50/51 to 55/56, transform IDs from 50/51 to 60/61. Non-overlapping ranges.

### Lesson for AI Models
**When assigning resource IDs in DynaCharting, ALL resource types share ONE namespace.** Before assigning any ID, mentally (or actually) check it against every other ID in the scene — across panes, layers, drawItems, buffers, geometries, AND transforms. Use non-overlapping ID ranges per type, or use a single incrementing counter.

Recommended ranges:
- Panes: 1-9
- Layers: 10-49 (or 10-59)
- Transforms: 60-69 (or 70-99)
- Buffers/Geometries/DrawItems: 100+ (interleaved in groups of 3: buf, geom, di)

---

## Bug 2: False Negative During Debugging

### Symptom
After fixing the ID collision, the developer reported "it is still not rendering." Investigation continued for an extended period.

### Root Cause
The fix was correct — pixel analysis confirmed all panes were rendering. The stale binary from before the fix was still running (or the browser was showing a cached frame from the old session). The server was not properly restarted after rebuilding.

### Lesson for AI Models
**After fixing a build-and-run issue, always:** (1) rebuild, (2) kill the old process, (3) restart the server, (4) hard-refresh the browser. Don't trust cached frames. Verify the fix by analyzing actual pixel output from the new binary before continuing to debug.

---

## Visual Issues Found in Review

These are layout/aesthetics problems, not bugs. They represent poor spatial reasoning when placing elements in clip space.

### 1. Y-axis labels bleed into adjacent panes
**What happened:** Price labels (44325, 43853...) and volume labels (14.0K, 11.4K...) were positioned at `clipX = LEFT_MAX + 0.01`, which placed them in the narrow gap between the left and right columns. They overlap with the right-side pane edges.

**Why:** The text overlay uses clip-space coordinates converted to pixel positions. The gap between columns (LEFT_MAX=0.28 to RIGHT_MIN=0.33) is only ~30 pixels. Labels placed at 0.29 in clip space land right in this gap, visually colliding with the right column.

**Correct approach:** Either place labels inside the pane (right-aligned against the left edge of the gap), or widen the gap to accommodate labels, or put labels on the left side of the pane.

### 2. Label collision at pane boundary
**What happened:** The lowest candle Y-axis label and the highest volume Y-axis label appear at nearly the same vertical position where the two panes meet.

**Why:** Both label sets independently divide their pane's Y-range into evenly spaced ticks. Neither accounts for the other. The boundary region gets crowded.

**Correct approach:** Skip the extreme tick closest to the boundary, or use a shared label region.

### 3. No visual separation between panes
**What happened:** All 5 panes have similar dark backgrounds (RGB differences of ~2-5 per channel). Without borders or dividers, pane boundaries are nearly invisible.

**Why:** Relied solely on subtle clear-color differences for separation. The human eye can't distinguish RGB(18,19,23) from RGB(17,18,22) at a glance.

**Correct approach:** Add thin line separators between panes (1-2px, subtle gray), or use more distinct background colors, or add visible borders using lineAA drawItems in a dedicated separator layer.

### 4. Pie legend positioning
**What happened:** Legend items are crammed below/left of the donut, extending into the gap between columns.

**Why:** Legend Y positions were computed relative to the pane's clip region but didn't account for the donut's actual size and position. The legend starts too far left.

**Correct approach:** Position legend items relative to the donut center, ensuring they stay within the pane's clip bounds with padding.

### 5. Right column feels cramped
**What happened:** Performance bars and sparklines have minimal padding. Text labels are close to pane edges.

**Why:** The right column spans clipX [0.33, 0.98] = 65% of clip width but only 39% of pixel width (due to clip space mapping). With three vertically stacked panes, each gets limited vertical space. Internal padding wasn't generous enough.

**Correct approach:** Either give the right column more width (move the split left), or reduce the number of right-side panes, or reduce content density (fewer bars, fewer sparklines).

---

## Key Spatial Reasoning Mistakes

These are the recurring patterns that caused most issues:

1. **Thinking in one coordinate space at a time.** The dashboard has three coordinate spaces: data space (timestamps, prices), clip space [-1,1], and pixel space (0-1200, 0-800). Mistakes happen when placing elements in clip space without mentally converting to pixel space to check if the result makes visual sense.

2. **Not budgeting for text.** GPU-rendered geometry (candles, bars, pie slices) is clipped by the scissor test. But text overlay labels are positioned in pixel space with no clipping — they can overlap anything. Always reserve explicit pixel-space regions for labels and keep geometry away from those regions.

3. **Treating gaps as free space.** The gap between the left and right columns (30px) was neither wide enough for labels nor narrow enough to be invisible. Gaps need to be intentionally sized: either 0 (panes touch) with separator lines, or wide enough (60-80px) to house axis labels.

4. **Ignoring the viewport-to-pixel ratio.** Clip space is uniform, but the viewport isn't square (1200x800). A 0.1 unit in clip-X is 60px, but a 0.1 unit in clip-Y is 40px. Layout constants chosen in clip space produce non-square results that look unbalanced.

---

## What the Binary Does Right

For training signal, these are patterns that worked well:

- **ID grouping by resource type**: Buffers, geometries, and drawItems allocated in groups of 3 (e.g., buf=106, geom=107, di=108) — easy to track, no collisions within a group
- **Pre-transformed clip-space geometry for static elements**: Sparklines and performance bars compute their clip-space positions at setup time, avoiding runtime transforms. Only the candle/volume panes need dynamic transforms for pan/zoom.
- **Donut tessellation with AA fringe**: The `tessellateDonutSliceAA` function generates clean anti-aliased geometry using the triAA@1 pipeline's per-vertex alpha
- **SimpleViewport abstraction**: Encapsulates data-range-to-clip-space transform math, pan, and zoom in one struct — clean separation of concerns
- **Linked pan/zoom**: Candle and volume panes share X-axis transform updates, so scrolling one scrolls both — correct UX pattern for financial charts
