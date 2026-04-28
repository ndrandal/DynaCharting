# Trial 095: Election Results

**Date:** 2026-03-22
**Goal:** Horizontal bar chart showing 5 parties sorted by vote share.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

800x500 viewport with one pane.

Five horizontal bars representing party vote shares, sorted descending:
Progressive (38.2%, blue), Conservative (31.5%, red), Liberal (15.8%, yellow), Green (9.1%, green), Independent (5.4%, gray). Bar length proportional to percentage. Rounded corners (4px).

Total: 18 unique IDs (1 pane, 1 layer, 5×(buf+geo+di)=15, 0 transforms)

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
- **Bar length scaling.** Percentages mapped to clip width: 38.2% → 1.30 clip units, 5.4% → 0.18 clip units.
- **Vertical stacking.** 0.34 clip units per row with 0.26 bar height gives 0.08 gap.
- **Party colors.** Traditional political colors: blue progressive, red conservative, yellow liberal, green green.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Sort bars by value for readability.** Descending order makes comparisons immediate.
2. **Use clip-space directly for static charts.** No transform needed when data is pre-computed.
