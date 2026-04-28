# Trial 247: Sheet Music

**Date:** 2026-03-22
**Goal:** 4 bars of music notation with 5 staff lines, bar lines, note heads as filled circles, and stems.
**Outcome:** 11 notes across 4 bars. 10 line segments (staff + bar lines), 11 note heads, 10 stems. 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 900x300. Cream/parchment background. 5 staff lines spanning full width.
4 measures with bar lines. Notes: Bar 1 (C E G E), Bar 2 (F A C5 A), Bar 3 (G E half notes), Bar 4 (C whole note).
Note heads are 8-segment circle fans (triSolid@1). Stems are vertical lines (lineAA@1).
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
- **Staff lines evenly spaced at 0.04 clip units.** Standard 5-line staff clearly visible.
- **Notes placed on correct lines/spaces.** Vertical position encodes pitch.
- **Whole note has no stem.** Correct notation convention.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Music notation maps to geometric primitives.** Note heads = circles, stems = lines, staff = horizontal lines.
2. **Small circle_fan segments (8) sufficient for note heads at this scale.**
