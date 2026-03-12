# Trial 062: QR Code

**Date:** 2026-03-12
**Goal:** Artistic QR code pattern (Version 1, 21×21 module grid) with 3 finder patterns, timing patterns, dark module, and deterministic data fill. Tests precise grid alignment of 441 binary-colored squares (instancedRect@1), finder pattern ring structure, and pixel-perfect binary pattern rendering on a 600×600 square viewport.
**Outcome:** All 441 modules at correct positions with correct colors. All 3 finder patterns structurally verified (49/49 each). 199 black + 242 white modules match expected counts exactly. Zero defects.

---

## What Was Built

A 600×600 viewport (square) with a single pane (white background):

**441 modules in a 21×21 grid (2 instancedRect@1 DrawItems):**

| Type | Color | Count | Layer |
|------|-------|-------|-------|
| White modules | #f8fafc | 242 | 11 |
| Black modules | #1e293b | 199 | 12 (front) |

Each module: 1×1 data-unit square at (col, row) to (col+1, row+1).

**QR structural elements:**

3 finder patterns (7×7 with outer black ring, middle white ring, inner 3×3 black):
- Top-left: rows 0–6, cols 0–6
- Top-right: rows 0–6, cols 14–20
- Bottom-left: rows 14–20, cols 0–6

Timing patterns: alternating black/white on row 6 (cols 8–12) and col 6 (rows 8–12).

Dark module at position (col=8, row=13).

Data area: remaining 283 modules filled with `(col × 7 + row × 13 + 42) % 3 == 0` → black, else white.

**Background (1 instancedRect@1, layer 10):**
White quiet zone covering [−4, −4] to [25, 25] (4-module margin around QR grid).

Data space: X=[−4, 25], Y=[−4, 25]. Transform 50: sx=sy=0.065517, tx=ty=−0.687931.

Layers: Background (10) → White modules (11) → Black modules (12).

Total: 14 unique IDs.

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

- **All 441 modules at correct grid positions with correct colors.** Every module verified against the expected grid (finder patterns + timing + dark module + data formula). 441/441 exact match.

- **All 3 finder patterns structurally correct.** Each 7×7 pattern has the standard QR ring structure: outer black border (24 modules), middle white ring (16 modules), inner 3×3 black core (9 modules). 49/49 correct for each of the 3 patterns.

- **Timing patterns alternate correctly.** Row 6 cols 8–12: B,W,B,W,B. Col 6 rows 8–12: B,W,B,W,B. Even positions black, odd positions white.

- **Dark module present at (8, 13).** The always-black module in real QR codes is correctly placed.

- **Data area pattern is deterministic and verified.** The formula `(col × 7 + row × 13 + 42) % 3 == 0` produces a pseudorandom-looking pattern that's fully reproducible and auditable.

- **Module counts match.** 199 black + 242 white = 441 = 21×21. Both counts verified against independent computation.

- **White quiet zone provides proper margin.** The background rect extends 4 modules beyond the QR grid in all directions, matching the QR standard quiet zone requirement.

- **Square viewport with equal sx=sy ensures square modules.** 600×600 with sx=sy=0.065517 means each module renders as a perfect square (~20×20 pixels).

- **Black modules on top of white creates clean rendering.** Layer 12 (black) draws over layer 11 (white), ensuring no z-fighting between adjacent modules.

- **All buffer sizes match vertex counts.** 3/3 geometries verified.

- **All 14 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Binary grid patterns use two instancedRect@1 DrawItems.** One for black modules, one for white. The grid position determines which list a module belongs to. Simple and efficient.

2. **QR finder patterns have a specific ring structure.** Outer border (1 module thick, black), middle ring (1 module thick, white), inner core (3×3, black). This creates the distinctive nested square appearance.

3. **Deterministic pseudorandom patterns are ideal for testing.** The formula `(col × 7 + row × 13 + 42) % 3 == 0` produces a pattern that looks random but is fully verifiable. Different prime multipliers prevent grid-aligned artifacts.

4. **Quiet zones need explicit background rectangles.** The 4-module white margin around the QR grid requires a background rect that extends beyond the module grid.

5. **14 IDs for 441 modules is highly efficient.** Grouping all black modules into one DrawItem and all white into another avoids the need for per-module resources.
