# Trial 256: Historical Timeline

**Date:** 2026-03-22
**Goal:** Horizontal timeline with 4 colored era bands, 10 event markers (alternating above/below), and connecting lines.
**Outcome:** 4 era bands (triGradient@1), 10 event circles, 21 connector/tick segments. 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 1000x400. Dark background. 4 semi-transparent era bands: Ancient (purple), Medieval (green), Renaissance (brown), Modern (blue).
Horizontal timeline line with 10 event markers alternating above and below. Vertical connector lines from timeline to each event. Tick marks at event positions.
Total: 13 unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Era bands provide visual context.** Semi-transparent rectangles behind timeline show periods.
- **Alternating event positions prevent overlap.** Odd events above, even below.
- **Connector lines link events to their timeline position.** Clear visual association.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Timelines are horizontal bar+dot compositions.** Main axis line + perpendicular connectors + endpoint markers.
2. **Alternating above/below prevents label crowding even without text rendering.**
