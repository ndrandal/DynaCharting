# Trial 058: Flame Chart

**Date:** 2026-03-12
**Goal:** Call stack profiler flame chart with 42 rectangles across 5 depth levels (instancedRect@1), warm color gradient from dark red (depth 0) to yellow (depth 4), parent-child nesting containment, and no within-depth overlaps. Tests contiguous/nested rectangle placement at scale, multi-level color coding, and hierarchical data representation on a 1000×500 viewport.
**Outcome:** All 42 rectangles at correct positions and Y ranges. All parent-child containment verified. No overlaps within any depth level. Zero defects.

---

## What Was Built

A 1000×500 viewport with a single pane (background #0f172a):

**5 depth levels of function call rectangles (5 instancedRect@1 DrawItems, rect4):**

| Depth | Color | # Rects | Y Range | Functions |
|-------|-------|---------|---------|-----------|
| 0 | #7f1d1d (dark red) | 1 | [0, 1.0] | main |
| 1 | #b91c1c (red) | 3 | [1.2, 2.2] | init, process, cleanup |
| 2 | #dc2626 (bright red) | 7 | [2.4, 3.4] | loadConfig, loadPlugins, parseInput, transform, writeOutput, flushBuffers, closeHandles |
| 3 | #f97316 (orange) | 13 | [3.6, 4.6] | readFile, validate, scanDir, sortPlugins, tokenize, buildAST, normalize, optimize, codegen, serialize, compress, syncDisk, closeFDs |
| 4 | #fbbf24 (yellow) | 18 | [4.8, 5.8] | openFD, stat, checkSchema, glob, lexScan, parseExpr, buildNodes, linkRefs, typeCheck, constFold, deadCode, regAlloc, emitAsm, linkObj, encodeJSON, gzip, writeChunk, fsync |

All bars 1.0 data units tall with 0.2 gap between levels. Alpha 0.9 for all.

**6 grid lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
5 vertical at X=20,40,60,80,100 spanning Y=[0,5.8]. 1 horizontal baseline at Y=0 spanning X=[0,100]. White, alpha 0.06, lineWidth 1. Layer 10.

**Text overlay:** Title, depth labels, and time axis labels (invisible in PNG capture).

Data space: X=[0, 100], Y=[0, 5.8]. Transform 50: sx=0.019, sy=0.327586, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Depth 0 (11) → Depth 1 (12) → Depth 2 (13) → Depth 3 (14) → Depth 4 (15).

Total: 26 unique IDs.

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

- **All 42 rectangles at correct time ranges and Y positions.** Every rect's (xMin, yMin, xMax, yMax) verified against the spec. Depth d has yMin=d×1.2, yMax=d×1.2+1.0. All 42/42 exact.

- **All parent-child containment verified.** Every depth-1 rect is within main(0–100). Every depth-2 rect is within its depth-1 parent. Every depth-3 rect is within its depth-2 parent. Every depth-4 rect is within its depth-3 parent. Full hierarchy verified.

- **No overlaps within any depth level.** Within each of the 5 depth levels, rectangles tile horizontally without overlapping. Gaps exist where no function call covers that time range (e.g., depth 3 gap at 92–95 between syncDisk and closeFDs, depth 4 gaps at 7–8, 10–15, 70–75, 90–95).

- **Warm color gradient communicates depth.** Dark red (bottom/root) → red → bright red → orange → yellow (top/leaf). The progression from cool-warm to hot-warm creates an intuitive "heat" metaphor where the deepest call stack burns hottest.

- **Gaps between depth levels are visible.** The 0.2 data-unit gap (Y) between each level creates clear visual separation of the 5 horizontal bands. At sy=0.327586, this is ~16 pixels of dark background.

- **Depth 4 gaps reveal call structure.** Empty spaces at depth 4 (e.g., no children for glob at 10–12, sortPlugins at 12–15) show where depth-3 functions have no sub-calls. These gaps are visible in the PNG and match the spec.

- **Transform math is exact.** sx=1.9/100=0.019 and sy=1.9/5.8≈0.327586 correctly map the data space to clip[−0.95, 0.95].

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 6/6 geometries verified.

- **All 26 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Flame charts map naturally to instancedRect@1.** Each function call is a rectangle with xMin=startTime, xMax=endTime, yMin=depth×rowHeight, yMax=depth×rowHeight+barHeight. One DrawItem per depth level enables per-depth coloring.

2. **Parent-child containment is the key structural invariant.** Every child's [xMin, xMax] must be within its parent's range. This is the primary audit check — if containment fails, the flame chart is structurally wrong.

3. **Gaps at deeper levels are expected and informative.** Not every parent function has children covering its entire time range. These gaps show where computation happens directly in the parent without sub-calls.

4. **0.2-unit gaps between depth levels provide clear separation.** With bars 1.0 units tall and 0.2 gaps, the 5 levels are easily distinguishable. The gap-to-bar ratio (1:5) is small enough to not waste space but large enough to see.

5. **Warm color gradients are natural for flame charts.** The "heat" metaphor (deeper = hotter) is the standard convention for flame charts and immediately communicates depth without labels.
