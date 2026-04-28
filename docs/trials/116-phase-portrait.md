# Trial 116: Phase Portrait

**Date:** 2026-03-22
**Goal:** 8 trajectory curves showing a damped harmonic oscillator in phase space (x vs dx/dt). Spiral-like paths converging to origin.
**Outcome:** Phase portrait of damped harmonic oscillator (damping=0.15). 8 trajectories from symmetric initial conditions, each with 200 Euler steps (dt=0.05) producing ~200 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 600x600. Phase portrait of damped harmonic oscillator (damping=0.15). 8 trajectories from symmetric initial conditions, each with 200 Euler steps (dt=0.05) producing ~200 lineAA@1 segments. Spirals converge to origin. Horizontal and vertical axis reference lines.

| DrawItem | Element | Color |
|---|---|---|
| 102-125 | 8 trajectories | 8 distinct colors |
| axes | x/v axes | dim gray |

Total: 34 unique IDs (1 pane, 2 layers, 1 transform, 10 buffers, 10 geometrys, 10 drawItems).

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
- **All 8 trajectories spiral inward toward the origin, matching the expected behavior of a stable damped system.** 
- **Phase portrait correctly plots position (x) on horizontal and velocity (dx/dt) on vertical axis.** 
- **Symmetric initial conditions produce symmetric spiral patterns around the origin.** 
- **Damping coefficient 0.15 produces visible but not overdamped spirals with ~5 visible loops.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Phase portraits use Euler integration — small dt (0.05) ensures trajectory accuracy over many steps.** 
2. **Damped harmonic oscillator has eigenvalues with negative real part, guaranteeing convergence to origin.** 
