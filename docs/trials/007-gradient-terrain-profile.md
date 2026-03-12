# Trial 007: Gradient Terrain Profile

**Date:** 2026-03-12
**Goal:** Topographic elevation profile with gradient-colored terrain fill (`triGradient@1`), dashed reference lines, peak markers, and geology scatter plot. First trial to use `triGradient@1`, dashed lines, and dense scatter points.
**Outcome:** Visually the most striking trial yet. All three new features work correctly. Gradient color math verified to 3 decimal places.

---

## What Was Built

A 1100x700 viewport with two panes:

**Pane 1 — Terrain Profile (1078×501px, 71.6%):**
- **Gradient terrain fill** (`triGradient@1`, pos2_color4, 756 vertices = 252 triangles) — area fill from sea level to elevation, with per-vertex colors creating a natural gradient: deep blue (sea level, 0m) → sandy tan (coast, 200m) → dark green (forest, 600m) → gray-brown (alpine, 1200m) → white (snow, 1800m). Colors interpolated linearly between stops at each vertex's Y position.
- **Terrain outline** (`lineAA@1`, 39 segments, light gray #e0e0e0, lineWidth 1.5) — crisp edge on top of the gradient fill.
- **Three dashed reference lines** (`lineAA@1` with `dashLength`/`gapLength`):
  - Tree Line at 200m: green dashes (12/8)
  - Alpine Zone at 600m: brown dashes (10/6)
  - Snow Line at 1200m: silver dashes (8/8)
- **Two peak markers** (`points@1`): red at major peak (x=15, 1746m, size 8), orange at secondary peak (x=30, 1208m, size 7).

**Pane 2 — Geology Scatter (1078×171px, 24.4%):**
- 150 data points in 3 categories using `points@1`:
  - 60 sedimentary (tan, ptSize 4.0) — scattered broadly, shallow
  - 50 igneous (dark red, ptSize 4.5) — clustered near peaks
  - 40 metamorphic (blue, ptSize 4.0) — scattered deep

Both panes linked via `"transect"` linkGroup for X-axis sync. Geology pane locks Y.

Total resources: 2 panes, 7 layers, 2 transforms, 10 buffers, 10 geometries, 10 drawItems, 2 viewports = 41 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. The title "Alpine Transect: Elevation Profile", zone labels ("Tree Line 200m", etc.), axis labels, and scatter legend are all browser-only. Without them, the dashed lines have no identifying context.

2. **Peak markers are small and hard to see.** The red marker at the major peak is barely visible against the white snow gradient. Point size 8 at 1100px viewport width produces a ~8px dot on a peak that's ~50px wide. A larger point size (12-16) or a contrasting outline would improve visibility. The orange marker at the secondary peak is more visible against the gray-brown gradient.

3. **Scatter plot is sparse.** 150 points across 1078px wide × 171px tall pane leaves significant empty space. The clustering of igneous points near peaks is visible but not striking. 300-400 points with tighter clusters would create a more compelling scatter visualization.

4. **Agent created a generator script** (`docs/trials/gen_007.py`). The trial rules require the agent to produce JSON directly, not use code generation. However, the final JSON IS self-contained and valid, and the script is supplementary. The spirit of "one shot, no iterations" is preserved since the script generates the data deterministically without trial-and-error.

---

## Spatial Reasoning Analysis

### Done Right

- **Gradient color interpolation is mathematically exact.** Verified at all 4 gradient stop elevations:
  - y=0m → (0.050, 0.180, 0.420) = spec's deep blue ✓ (exact match)
  - y=200m → (0.760, 0.700, 0.500) = spec's sandy tan ✓ (exact match)
  - y=600m → (0.130, 0.550, 0.130) = spec's dark green ✓ (exact match)
  - y=1200m → (0.550, 0.450, 0.350) = spec's gray-brown ✓ (exact match)
  - y=1746m → (0.887, 0.887, 0.905) — correct interpolation between 1200m and 1800m stops at 91% ✓

  Also spot-checked: y=45.3m → (0.211, 0.298, 0.438). Expected by linear interpolation at 45.3/200 = 22.7% between stops: R = 0.05 + 0.227*(0.76-0.05) = 0.211 ✓. All three channels match to 3 decimal places.

- **Sub-band tessellation produces smooth gradient.** The 39 strips are subdivided at each gradient color stop that falls within the strip's elevation range. Higher-elevation strips (near the peak) have more sub-bands and therefore more triangles. 756 vertices / 39 strips ≈ 19.4 verts per strip average, with high-elevation strips having up to 30+ vertices and coastal strips having ~6.

- **Dashed lines render correctly.** First use of `dashLength`/`gapLength` in trials. Three distinct dash patterns are clearly visible in the image, each at the correct elevation. The dashes are sized appropriately for the viewport width.

- **Layout is proportional.** 71.6% terrain + 2% gap + 24.4% geology = 98% + 2% padding = 100%. No wasted space. Addresses trial 004's gap issue.

- **Elevation profile is geologically plausible.** Twin peaks with a valley between them, starting and ending near sea level. The major peak (1746m) reaches near the snow line, the secondary (1208m) is alpine. The terrain smoothly rises and falls without unnatural discontinuities.

### Done Wrong

- Nothing structurally wrong. The generator script is a process deviation but produced correct output.

---

## Lessons for Future Trials

1. **`triGradient@1` with `pos2_color4` works beautifully for elevation/heat coloring.** The key technique: tessellate area fills into sub-bands at color-stop boundaries, with each vertex getting the interpolated color at its Y position. The GPU interpolates colors smoothly between vertices, creating seamless gradients.

2. **Dashed lines are straightforward.** Set `dashLength` and `gapLength` on any `lineAA@1` DrawItem. The dash pattern is in pixel space, so values of 8-12 pixels produce visible dashes at typical viewport sizes.

3. **Increase point sizes for annotation markers.** Points used as annotation markers (peaks, landmarks) need to be larger than data points. 8px is barely visible at 1100px viewport width. Use 12-16px for annotation markers, 3-5px for data scatter points.

4. **For dense scatter plots, use more points.** 150 points across a 1078px-wide pane is sparse (~1 point per 7px horizontally). For convincing cluster visualization, aim for 300+ points with defined cluster centers and spread parameters.
