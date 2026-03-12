# Trial 020: Lollipop Chart

**Date:** 2026-03-12
**Goal:** Horizontal lollipop chart with 10 programming language satisfaction scores — thin stem lines (lineAA@1) with colored circle endpoints (triAA@1). First trial combining both AA pipelines, testing extreme aspect correction (X:Y data range ratio ~10:1).
**Outcome:** All 10 lollipop positions, circle radii, and stem endpoints are exact. Circles are perfectly circular despite a 6.926× aspect correction. One procedural violation, one major meta-finding.

---

## What Was Built

A 1000×700 viewport with a single pane (976×676px, 12px margins):

**10 stem lines** (single lineAA@1 DrawItem, rect4 format, 10 instances):
Each horizontal segment from (0, row) to (score, row). Gray, alpha 0.3, lineWidth 1.5.

**10 circle heads** (10 triAA@1 DrawItems, pos2_alpha format, 144 vertices each):

| Language | Score | Row | Color | Pixel radius |
|----------|-------|-----|-------|-------------|
| C++ | 48 | 1 | Orange-red | 21.5px |
| PHP | 55 | 2 | Pink | 21.5px |
| Java | 65 | 3 | Amber | 21.5px |
| C# | 72 | 4 | Purple | 21.5px |
| Swift | 76 | 5 | Coral | 21.5px |
| Kotlin | 78 | 6 | Light blue | 21.5px |
| Go | 81 | 7 | Cyan | 21.5px |
| TypeScript | 84 | 8 | Blue | 21.5px |
| Python | 88 | 9 | Green | 21.5px |
| Rust | 92 | 10 | Orange | 21.5px |

Circle radii: X=2.424, Y=0.350 data units. Aspect correction factor: 6.926 (px_per_dy/px_per_dx = 61.455/8.873).

Data space: X=[−5, 105], Y=[0, 11]. Transform: sx=0.01775, sy=0.17558, tx=−0.88727, ty=−0.96571.

16 angular segments per circle. Core: 48 vertices (alpha=1). Fringe: 96 vertices (alpha 1→0). Total: 144 per circle.

Layers: stems (10, behind), circles (11, on top).

Total: 1 pane, 2 layers, 1 transform, 11 buffers, 11 geometries, 11 drawItems, 1 viewport = 37 IDs.

---

## Defects Found

### Critical

None.

### Major

1. **One-shot rule violated.** The agent initially wrote lineAA@1 with `pos2_clip` format (as instructed in the spec), which failed engine validation. The agent then corrected to `rect4` format and re-rendered. The trial rules explicitly state: "ONE SHOT. Do not modify the JSON after writing it." However, this violation led to the discovery in the meta-finding below.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Meta-Finding: lineAA@1 Format Correction

**The "lineAA@1 format mismatch" noted in trials 009, 011, 012, 014, and 018 was a false positive in every audit.**

Source code confirmation (`core/src/pipelines/PipelineCatalog.cpp:22`):
```cpp
reg("lineAA", 1, VertexFormat::Rect4, DrawMode::InstancedTriangles, 6);
```

`lineAA@1` uses **`rect4`** format with **instanced triangle** rendering (6 vertices per instance). Each "vertex" is a line segment endpoint pair `[x0, y0, x1, y1]`, and the GPU expands each instance into a 2-triangle quad with AA fringe.

This means:
- The builder agents were **correct** in all prior trials when they used `rect4` for `lineAA@1`
- The auditor (me) was **wrong** in flagging this as a format mismatch across 5 trials
- `CHART_AUTHORING.md` correctly documents `lineAA@1` as using `rect4` format
- The spec for this trial incorrectly instructed `pos2_clip`, which the engine rejected

**All prior audit entries about "lineAA@1 format mismatch" should be considered errata.**

---

## Spatial Reasoning Analysis

### Done Right

- **All 10 circle centers are exact.** C++ at (48,1), PHP at (55,2), ..., Rust at (92,10). All verified to sub-pixel precision.

- **All 10 circles are perfectly circular.** Despite a 6.926× aspect correction (the most extreme in any trial), pixel radii are 21.5px in both X and Y for all 10 circles. The X data radius (2.424) × px_per_dx (8.873) = Y data radius (0.350) × px_per_dy (61.455) = 21.5px.

- **All 10 stem segments are correct.** Each horizontal line runs from x=0 to x=score at the correct Y row. rect4 format: (0, row, score, row) for each segment.

- **Fringe is exactly 2.5px.** Verified on Rust: X fringe = 0.281763 data units × 8.873 px/unit = 2.50px. The fringe is correctly aspect-corrected in both X and Y directions.

- **All vertex formats are correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓. Zero format mismatches.

- **All vertex counts match.** Stems: 40 floats / 4 = 10 instances. Circles: 432 floats / 3 = 144 vertices each. All 11 geometries verified.

- **Layer ordering is correct.** Stems on layer 10 (behind), circles on layer 11 (on top). In the image, circles are drawn over the stem endpoints.

- **All 37 IDs unique.** Systematic allocation with no collisions.

### Done Wrong

- **The spec gave the wrong format for lineAA@1.** The orchestrator specified `pos2_clip` based on incorrect assumptions carried through 5+ prior audits. The agent corrected this based on engine validation.

---

## Lessons for Future Trials

1. **lineAA@1 uses rect4 format, not pos2_clip.** Each instance is a line segment defined by (x0, y0, x1, y1). The GPU generates 6 vertices (2 triangles) per instance for the AA quad. This has been confirmed by the engine source code and should never be flagged as a format mismatch again.

2. **Extreme aspect correction works.** A 6.926× ratio between X and Y data-unit-to-pixel scales is handled correctly by both the circle tessellation and the fringe calculation. The key: compute px_per_dx and px_per_dy separately, derive the correction ratio, and apply it to both the shape radii and the fringe offsets.

3. **Lollipop charts combine lineAA@1 and triAA@1 effectively.** One lineAA@1 DrawItem handles all 10 stems (10 instances), while individual triAA@1 DrawItems give each circle a distinct color. The layer ordering ensures circles render on top of stems.

4. **When engine validation fails, it reveals truth.** The one-shot violation was justified by discovering that the orchestrator's format assumption was wrong. Future specs must respect the engine's actual pipeline catalog.
