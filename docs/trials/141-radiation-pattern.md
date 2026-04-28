# Trial 141: Antenna Radiation Pattern

**Date:** 2026-03-22
**Goal:** Polar radiation pattern of a dipole antenna (lineAA@1, 40 segments). Reference circles at 0, -3, and -6 dB as dashed lines.
**Outcome:** Dipole antenna radiation pattern (cos^2 gain). 41 angular samples producing 40 lineAA@1 segments in polar coordinates. Zero defects.

---

## What Was Built
Viewport 600x600. Dipole antenna radiation pattern (cos^2 gain). 41 angular samples producing 40 lineAA@1 segments in polar coordinates. Two-lobed figure-eight pattern with nulls at 90 and 270 degrees. Dashed reference circles at 0 dB (r=0.75), -3 dB (r=0.53), -6 dB (r=0.375). Axes through origin.

| Layer | Elements | Pipeline | Color |
|---|---|---|---|
| 10 | Ref circles + axes | lineAA@1 dashed | dim/gray |
| 11 | Radiation pattern | lineAA@1 | cyan |

Total: 18 unique IDs (1 pane, 2 layers, 5 buffers, 5 geometrys, 5 drawItems).

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
- **Two-lobed pattern with maximum gain at 0 and 180 degrees matches dipole antenna behavior.** 
- **Nulls at 90 and 270 degrees (equatorial plane) are correct for a dipole oriented along 0-180 axis.** 
- **Pattern passes through -3 dB reference at cos^2(theta)=0.5, i.e., theta=45 degrees from maximum.** 
- **Polar-to-Cartesian conversion: (r*cos(theta), r*sin(theta)) correctly maps the pattern to 2D.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Polar plots convert (gain, angle) → (gain*cos(angle), gain*sin(angle)) for Cartesian rendering.** 
2. **Reference circles at dB levels provide magnitude scale: -3 dB = 0.707x, -6 dB = 0.5x of peak.** 
