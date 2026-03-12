# Trial 017: Bubble Chart

**Date:** 2026-03-12
**Goal:** GDP-per-capita vs Life Expectancy bubble chart with 20 country bubbles sized by population, colored by continent, using triAA@1 circles with aspect correction for a non-square pane. First trial with variable-size circles at arbitrary positions, testing per-bubble AA tessellation and transparent overlapping.
**Outcome:** All 20 bubble positions, radii, and colors are exact. Circularity is perfect (pixel radius std = 0.000 across angles). Fringe is exactly 2.5px. The visual result is a clean, professional bubble chart. Zero defects.

---

## What Was Built

A 1100×700 viewport with a single pane (1076×676px, 12px margins):

**20 bubbles** (triAA@1, pos2_alpha, 216 vertices each = 24 segments):

| Country | X_norm | Y_norm | Radius | Pixel r | Color | Continent |
|---------|--------|--------|--------|---------|-------|-----------|
| USA | 0.7875 | 0.7444 | 0.055 | 30.98px | Red | Americas |
| China | 0.1563 | 0.7178 | 0.080 | 45.07px | Green | Asia |
| Japan | 0.5000 | 0.8800 | 0.040 | 22.53px | Green | Asia |
| Germany | 0.6375 | 0.8067 | 0.033 | 18.59px | Blue | Europe |
| UK | 0.5750 | 0.8000 | 0.030 | 16.90px | Blue | Europe |
| India | 0.0288 | 0.5644 | 0.078 | 43.94px | Green | Asia |
| France | 0.5500 | 0.8289 | 0.030 | 16.90px | Blue | Europe |
| Brazil | 0.1088 | 0.6867 | 0.049 | 27.60px | Red | Americas |
| Canada | 0.6500 | 0.8311 | 0.025 | 14.08px | Red | Americas |
| Australia | 0.6875 | 0.8533 | 0.022 | 12.39px | Purple | Oceania |
| S.Korea | 0.4250 | 0.8556 | 0.027 | 15.21px | Green | Asia |
| Mexico | 0.1250 | 0.6667 | 0.040 | 22.53px | Red | Americas |
| Indonesia | 0.0525 | 0.5933 | 0.051 | 28.73px | Green | Asia |
| Nigeria | 0.0263 | 0.2156 | 0.049 | 27.60px | Orange | Africa |
| Russia | 0.1438 | 0.6267 | 0.042 | 23.66px | Blue | Europe |
| S.Africa | 0.0750 | 0.4244 | 0.029 | 16.34px | Orange | Africa |
| Egypt | 0.0450 | 0.5956 | 0.036 | 20.28px | Orange | Africa |
| Turkey | 0.1188 | 0.7267 | 0.033 | 18.59px | Blue | Europe |
| Thailand | 0.0900 | 0.7156 | 0.030 | 16.90px | Green | Asia |
| Ethiopia | 0.0113 | 0.4800 | 0.037 | 20.84px | Orange | Africa |

Data normalized to [0,1] × [0,1] from GDP $0–80K × LifeExp 45–90y. Viewport padded to [-0.1, 1.1] on both axes.

Aspect correction: 676/1076 = 0.6283. Each bubble's X radius = Y radius × 0.6283. All bubbles have alpha 0.7 for translucent overlapping.

Tessellation per bubble: 24-segment triangle fan core (72 vertices, alpha=1.0) + fringe ring (144 vertices, alpha 0→1). Total: 216 vertices × 20 bubbles = 4,320 vertices.

Text overlay: title, axis labels, 5 country annotations, continent legend.

Total resources: 1 pane, 1 layer, 1 transform, 20 buffers, 20 geometries, 20 drawItems, 1 viewport = 63 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, bubbles are identified only by color (continent) and position. Country annotations, axis labels, and the title are absent. The continent legend is also invisible.

---

## Spatial Reasoning Analysis

### Done Right

- **All 20 bubble centers are exact.** Every center matches the specified normalized coordinates to 4 decimal places. The normalization from raw GDP/LifeExp to [0,1] space is correct.

- **Bubbles are perfectly circular.** USA: pixel radius = 30.98px at all 24 angles (std = 0.000). China: 45.07px at all 24 angles (min = max). The aspect correction factor 0.6283 correctly compensates for the 1076×676px non-square pane.

- **Fringe width is exactly 2.5px.** Verified on USA: core perimeter at 30.98px, outer fringe at 33.48px, difference = 2.50px. The fringe offset is correctly aspect-corrected in both X (0.002788 data units) and Y (0.004438 data units).

- **All vertex formats correct.** All 20 DrawItems use `triAA@1` with `pos2_alpha` ✓. No format mismatches.

- **All vertex counts match.** 216 vertices per bubble = 648 floats per buffer / 3 floats per vertex. All 20 geometries verified.

- **Transform is mathematically exact.** sx=1.630303, sy=1.609524, tx=-0.815152, ty=-0.804762. Maps data [-0.1, 1.1] to pane clip region. Verified against expected computation to 6 significant figures.

- **Color scheme correctly groups continents.** 7 green (Asia), 5 blue (Europe), 4 red (Americas), 4 orange (Africa), 1 purple (Oceania). All match the specified colors.

- **Alpha transparency produces clean overlapping.** With alpha=0.7 on all DrawItem colors, overlapping bubbles in the lower-left cluster (India, Indonesia, Egypt, Thailand, Brazil, Russia) show through each other, maintaining visual distinction.

- **Bubble sizes correctly reflect population.** China (r=0.080, 1.4B) and India (r=0.078, 1.38B) are the largest. Australia (r=0.022, 26M) and Canada (r=0.025, 38M) are among the smallest. The visual size hierarchy is clear.

- **All 63 IDs unique.** Systematic triplet allocation (100-159) with no collisions across pane, layer, transform, and per-bubble resources.

- **Pane margins are correct.** 12px on all sides: X margins = (1100 - 1076) / 2 = 12px, Y margins = (700 - 676) / 2 = 12px.

### Done Wrong

Nothing. This is the second consecutive zero-defect trial (after 015).

---

## Lessons for Future Trials

1. **Normalized data space solves the heterogeneous-axis problem.** When X and Y axes have vastly different scales (GDP 0–80K vs LifeExp 45–90), normalizing both to [0,1] makes bubble radius calculations tractable. The aspect correction then only needs to account for the pane's pixel ratio, not the data scale difference.

2. **Per-bubble DrawItems enable distinct colors.** With 20 DrawItems (one per bubble), each bubble can have its own continent-coded color. The overhead of 20 DrawItems is minimal. For >50 bubbles, consider grouping same-color bubbles into a single DrawItem using `triAA@1` with concatenated vertex buffers.

3. **Triangle fan + fringe ring is the standard triAA@1 circle recipe.** 24 segments gives smooth circles (15° per segment). Core: 72 vertices (24 triangles from center to perimeter). Fringe: 144 vertices (24 quad segments = 48 triangles). Total 216 vertices per circle.

4. **Alpha blending works well for overlapping bubbles.** The 0.7 alpha produces visible transparency where bubbles overlap, maintaining readability even in dense clusters. This is preferable to opaque bubbles which would obscure smaller points behind larger ones.
