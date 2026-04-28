# Trial 108: Kanban Columns

**Date:** 2026-03-22
**Goal:** 4-column Kanban board with 12 task cards distributed across columns.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

1000x600 viewport with one pane and two layers.

Four columns (Backlog, InProgress, Review, Done) with dark rounded backgrounds. 12 colored task cards distributed: Backlog (4 blue), InProgress (3 orange), Review (2 purple), Done (3 green). Cards have rounded corners (5px) and are inset within column backgrounds. Columns on layer 10, cards on layer 11.

Total: 18 unique IDs (1 pane, 2 layers, 5×(buf+geo+di) groups = 15)

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
- **Column layout.** 4 columns of 0.42 width with 0.06 gaps fill the [-0.9, 0.9] range.
- **Card inset.** Cards are 0.06 narrower than columns, creating a visual margin.
- **Layer ordering.** Column backgrounds behind cards ensures cards are visible.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Inset cards within columns.** The 0.03 padding on each side creates a contained appearance.
2. **Color-code by column.** Different card colors per column aids quick visual scanning.
