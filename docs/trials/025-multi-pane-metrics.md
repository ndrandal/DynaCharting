# Trial 025: Multi-Pane Metrics Dashboard

**Date:** 2026-03-12
**Goal:** Three vertically stacked panes (CPU / Memory / Latency) sharing a linked X axis with independent Y ranges. Each pane has an area fill, grid lines, and a data line. First trial testing multi-pane layout with inter-pane gaps, linked viewports, and proportional pane sizing (45%/30%/25%).
**Outcome:** All 57 line segments, 57 area triangulations, 10 grid lines, 3 transforms (shared sx/tx), and 3 pane regions are exact. Pane proportions, gaps, and viewport linkage all verified. Zero defects.

---

## What Was Built

A 1000×750 viewport with three vertically stacked panes (12px margins, 5px inter-pane gaps):

**Pane 1 — CPU Usage (top, 45%, 322px):**
- Cyan (#22d3ee) line and area fill, Y=[0, 100]%
- Grid at 25%, 50%, 75%
- clipY [0.1088, 0.968]

**Pane 2 — Memory (middle, 30%, 215px):**
- Green (#34d399) line and area fill, Y=[1800, 3600] MB
- Grid at 2000, 2500, 3000, 3500 MB
- clipY [−0.477, 0.095]

**Pane 3 — Latency (bottom, 25%, 179px):**
- Orange (#f59e0b) line and area fill, Y=[0, 300] ms
- Grid at 75, 150, 225 ms
- clipY [−0.968, −0.491]

All panes share: clipX [−0.976, 0.976], data X=[−1, 20], 20 data points at T=0..19.

**Per pane (×3 = 9 DrawItems):**
- Area fill (triSolid@1, pos2_clip, 114 vertices = 19 segments × 6 verts). Alpha 0.15.
- Grid lines (lineAA@1, rect4, 3–4 instances). White, alpha 0.06.
- Data line (lineAA@1, rect4, 19 segments). lineWidth 2, alpha 1.0.

Area baselines: CPU=0, Memory=1800, Latency=0.

**Three transforms (shared X mapping):**
| Transform | sx | sy | tx | ty |
|-----------|-----|-----|-----|-----|
| 50 (CPU) | 0.092952 | 0.008592 | −0.883048 | 0.1088 |
| 51 (Mem) | 0.092952 | 0.000318 | −0.883048 | −1.050 |
| 52 (Lat) | 0.092952 | 0.001591 | −0.883048 | −0.968 |

**Three linked viewports** in linkGroup "time" — X pan/zoom syncs across panes.

Layers per pane: area (N0) → grid (N1) → line (N2). IDs: pane 1 layers 10-12, pane 2 layers 20-22, pane 3 layers 30-32.

Total: 3 panes, 9 layers, 3 transforms, 9 buffers, 9 geometries, 9 drawItems, 3 viewports = 42 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 57 line segments are exact.** 19 per pane, all verified against the data table. CPU: (0,35)→(1,42)→...→(19,45). Memory: (0,2100)→...→(19,2150). Latency: (0,45)→...→(19,48). Zero errors.

- **All area fills use correct baselines.** CPU area fills down to Y=0. Memory area fills down to Y=1800 (the Y-range minimum, not zero — correct since memory doesn't start at zero). Latency fills down to Y=0. Verified from buffer data.

- **Area triangulation is correct.** Each segment produces 6 vertices (2 triangles): tri1=(xi,yi),(xi,baseline),(xi+1,yi+1); tri2=(xi+1,yi+1),(xi,baseline),(xi+1,baseline). Spot-checked first 2 segments of CPU — exact.

- **All three transforms share identical sx and tx.** sx=0.092952381, tx=−0.883047619. This ensures the X axis maps identically across all three panes, so the same time index appears at the same horizontal position in each pane.

- **Each transform correctly maps its Y range to its pane's clipY bounds.**
  - CPU: Y=0→clipY=0.109, Y=100→clipY=0.968 ✓
  - Memory: Y=1800→clipY=−0.477, Y=3600→clipY=0.095 ✓
  - Latency: Y=0→clipY=−0.968, Y=300→clipY=−0.491 ✓

- **Pane proportions are exact.** CPU=322.2px (45.0%), Memory=214.8px (30.0%), Latency=179.0px (25.0%). Total pane height 716.0px + 10px gaps + 24px margins = 750px ✓.

- **Inter-pane gaps are exactly 5px.** Gap 1 (pane 2→1): 0.01333 clip = 5.0px. Gap 2 (pane 3→2): 0.01334 clip = 5.0px. The 0.001px difference is from rounding — imperceptible.

- **Panes do not overlap.** Pane 3 top (−0.491) < Pane 2 bottom (−0.477) with 5px gap. Pane 2 top (0.095) < Pane 1 bottom (0.109) with 5px gap.

- **All viewports are linked.** All three in linkGroup "time" with matching X ranges [−1, 20]. Interactive pan/zoom on X axis will sync across panes.

- **Grid lines span the full data X range.** All grid lines go from X=−1 to X=20, matching the viewport X range.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓. Zero mismatches.

- **All vertex counts match.** Area fills: 228/2=114=19×6 ✓. CPU grid: 12/4=3 ✓. Memory grid: 16/4=4 ✓. Latency grid: 12/4=3 ✓. Data lines: 76/4=19 ✓ (all three).

- **Layer ordering creates correct depth.** Area (behind) → grid → line (on top) per pane. Grid lines are visible over the semi-transparent area fill.

- **Color coding is distinct.** Cyan (CPU), green (Memory), orange (Latency). All hex-to-float conversions verified.

- **All 42 IDs unique.** Systematic allocation with triplet grouping per pane. No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Multi-pane layout requires careful clip-space arithmetic.** Total available height = viewport clip range − margins − gaps. Then divide proportionally. All pane regions must sum to the available height with gaps between them.

2. **Shared sx/tx is essential for linked X axes.** If two panes show the same time range, their transforms must have identical sx and tx values. Only sy and ty differ (different Y data ranges mapped to different pane heights).

3. **Linked viewports enable synchronized interaction.** With all three viewports in the same linkGroup, X-axis pan/zoom in any pane propagates to the others. This is the primary mechanism for coordinated multi-pane dashboards.

4. **Area fill baselines should match the Y-range minimum, not always zero.** Memory starts at Y=1800, so its area fill goes down to 1800 (the pane floor), not to 0 (which would be below the visible area). The baseline should be the lowest Y value that the transform maps to the pane's clipYMin.

5. **triSolid@1 area fills at alpha 0.15 create effective transparent fills.** The low alpha creates a "filled area" effect that highlights the region under the curve without obscuring the grid lines behind it. This is a standard chart pattern.
