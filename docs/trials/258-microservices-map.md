# Trial 258: Microservices Map

**Date:** 2026-03-22
**Goal:** Architecture diagram with 8 service boxes (API gateway, auth, users, orders, payments, inventory, notifications, analytics) and 8 directed connections with arrowheads.
**Outcome:** 8 service boxes, 8 connection lines, 8 arrowheads. 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark background. 8 service boxes (instancedRect@1, cornerRadius=6).
API gateway at top center, 4 main services in middle row, 3 backend services at bottom.
8 directed connections with line + triangle arrowhead showing dependency flow.
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
- **Gateway at top center establishes entry point.** Fan-out to 4 services below.
- **Three-tier layout.** Gateway -> main services -> backend services.
- **Arrowheads indicate dependency direction.** From caller to callee.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Architecture diagrams: position services by tier/layer.** Top=entry, middle=domain, bottom=infrastructure.
2. **Arrow_head helper reusable for any directed graph.**
