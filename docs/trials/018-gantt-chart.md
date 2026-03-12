# Trial 018: Gantt Chart

**Date:** 2026-03-12
**Goal:** Twelve-task project timeline Gantt chart with horizontal bars color-coded by status (complete/in-progress/planned), a dashed "Today" marker line, and week labels. First trial with horizontal bar layout using instancedRect@1, testing many bars at varied X/Y positions.
**Outcome:** Classic Gantt chart with correct diagonal cascade pattern. All 12 bar positions, the Today marker, and text label positions are mathematically exact. One minor recurring defect.

---

## What Was Built

A 1200×600 viewport with a single pane (1176×576px, 12px margins):

**12 task bars across 3 instancedRect@1 DrawItems:**

| Task | Weeks | Row | Width | Color |
|------|-------|-----|-------|-------|
| Requirements | 0–3 | 12 | 160px | Green (complete) |
| UI Design | 1–5 | 11 | 214px | Green |
| DB Schema | 2–4 | 10 | 107px | Green |
| Auth Module | 3–8 | 9 | 267px | Green |
| API Layer | 4–10 | 8 | 321px | Blue (in progress) |
| Frontend Core | 5–12 | 7 | 374px | Blue |
| Data Pipeline | 7–13 | 6 | 321px | Blue |
| Integration | 10–15 | 5 | 267px | Gray (planned) |
| Testing | 12–17 | 4 | 267px | Gray |
| Performance | 14–18 | 3 | 214px | Gray |
| Documentation | 16–19 | 2 | 160px | Gray |
| Launch | 18–20 | 1 | 107px | Gray |

All bars: 27px tall (0.6 data units), with 3px corner radius.

**Today marker:** White dashed vertical line at week 9 (pixel 546.5, 45.5% from left). lineAA@1, lineWidth 2, dashLength 8, gapLength 5, alpha 0.6.

Data space: X=[−1, 21] (weeks), Y=[0, 13] (rows). Transform: sx=0.08909, sy=0.14769, tx=−0.89091, ty=−0.96.

Text overlay: title, 12 task names (right-aligned at clipX=−0.92), 11 week labels (W0–W20), "Today" annotation.

Total: 1 pane, 2 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems, 1 viewport = 16 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **lineAA@1 geometry declares rect4 format.** The Today line (geom 110) uses `rect4` instead of `pos2_clip`. This is the persistent format mismatch that has appeared in trials 009, 011, 012, 014, and now 018. The engine renders correctly regardless — the Today line is clearly visible as a dashed vertical line at the correct position.

2. **Text labels invisible in PNG capture.** Known limitation. Without labels, the chart has no task names, week numbers, or title. The three color groups (green/blue/gray) and the Today marker are the only visual cues to the data.

---

## Spatial Reasoning Analysis

### Done Right

- **All 12 bar positions are exact.** Each bar's X range matches its week span, and its Y range is centered on its row (row ± 0.3). Verified all 12 bars against the specification — zero position errors.

- **Bar widths are proportional to task duration.** 2-week tasks (DB Schema, Launch) = 107px, 3-week tasks = 160px, ..., 7-week task (Frontend Core) = 374px. Width = weeks × 53.5px/week, consistent across all bars.

- **All bars have uniform height.** 27px (0.6 data units × scale factor). The consistent height with the cascading start/end times creates the classic Gantt diagonal pattern.

- **Today line is correctly positioned.** Week 9 → clipX = −0.0891 → pixel 546.5. In the image, the dashed line bisects the in-progress (blue) tasks exactly where expected — API Layer extends 1 week past Today, Frontend Core extends 3 weeks past, Data Pipeline extends 4 weeks past.

- **Color grouping matches status.** Green (complete, rows 9–12) → Blue (in progress, rows 6–8) → Gray (planned, rows 1–5). The status boundary aligns with the Today line: all green bars end before week 9, all gray bars start after week 9.

- **Transform is mathematically exact.** sx=0.089091, sy=0.147692, tx=−0.890909, ty=−0.96. All verified to 9 significant figures against expected computation from viewport bounds and pane clip region.

- **Text label positions are exact.** Task name clipY values match row positions: Requirements (row 12) → 0.8123, Launch (row 1) → −0.8123. Week label clipX values match week positions: W0 → −0.891, W10 → 0.000, W20 → 0.891. All verified.

- **Vertex counts match buffer sizes.** Green: 16 floats / 4 = 4 instances, Blue: 12/4 = 3, Gray: 20/4 = 5, Today: 4/4 = 1. All correct for their declared formats.

- **All 16 IDs unique.** Systematic allocation: pane 1, layers 10-11, transform 50, triplets 100-111.

- **Corner radius is a nice addition.** The 3px cornerRadius on all bar DrawItems adds visual polish (visible as subtle rounding in the image).

### Done Wrong

- **lineAA@1 format mismatch persists.** The agent explicitly stated this was "consistent with the authoring guide and all working chart examples." This confirms the agent has internalized rect4 as the correct format for lineAA@1, which is incorrect (pos2_clip is the right format). The visual rendering is unaffected.

---

## Lessons for Future Trials

1. **Gantt charts are natural for instancedRect@1.** Group tasks by status into 3 DrawItems (4+3+5 instances), each with a distinct color. This is more efficient than one DrawItem per task (which would need 12).

2. **Horizontal bars with cascading start times create the Gantt pattern.** The key layout: tasks ordered top-to-bottom by start date, with earlier tasks at the top. The diagonal cascade from top-left to bottom-right is immediately recognizable.

3. **The "Today" marker adds critical context.** Without it, there's no visual anchor for which tasks are overdue, in progress, or upcoming. The dashed line at week 9 clearly separates completed from planned work.

4. **The lineAA@1 rect4 format mismatch is now a 6-trial pattern.** The builder agent consistently believes rect4 is correct for lineAA@1. Future specs should include an explicit warning: "lineAA@1 MUST use pos2_clip format, NOT rect4."
