# SPEC: GLFW live-window milestone (first interactive C++ demo)

**Slug:** glfw-live-window · **Date:** 2026-05-01 · **Status:** Approved
**Repo:** DynaCharting · **Linear:** ENC-91

## Problem
ROADMAP.md "Next Up" lists three infrastructure items (CI/CD, GLFW
live window, embedding example). None have a SPEC or PLAN. Without the
GLFW window, every C++ rendering change is verified through OSMesa
offscreen tests — fast, but no human-in-the-loop visual feedback. The
gap blocks the rest of the infra roadmap (CI/CD targets need
something to demo; the embedding example needs a window to embed in).

## Proposed change
Add a minimal `apps/demo-glfw/` C++ binary that opens a GLFW window,
hosts a `GlContext`, drives the existing `Renderer`, and renders a
canned scene (the same scene as the OSMesa D2.1 smoke test). No new
features — purely a new presentation surface for the existing core.

## Scope
- New CMake target `apps/demo-glfw/CMakeLists.txt` — links `dc`,
  `dc_gl`, GLFW (vendored under `third_party/` like GLAD / RapidJSON).
- `apps/demo-glfw/main.cpp` — boots GLFW, creates a window
  (1280×800 default), creates `GlfwGlContext` (sibling of
  `OsMesaGlContext`), instantiates `Renderer`, loads the canned scene
  from `core/tests/fixtures/d21_minimal_scene.json` (or wherever the
  D2.1 scene lives), renders in a frame loop.
- New `core/include/dc/gl/GlfwGlContext.hpp` + impl — concrete
  `GlContext` implementation using GLFW's window-bound GL context.
- Hot keys: `R` reload scene, `Esc` quit.
- README / ROADMAP update: mark GLFW milestone Done; describe how to
  run.

## Non-goals
- **Not interactive editing.** Display only; no UI, no inspector, no
  recipe authoring.
- **Not WebGL parity.** `apps/demos/hello-engine` stays the TS demo.
- **Not bundled binaries.** Build-from-source only.
- **Not Windows/macOS first-class.** Linux primary; the others should
  build but are best-effort.
- **Not CI integration.** GLFW windowing under headless CI is its own
  problem; track separately.

## Acceptance criteria
1. `cmake -B build && cmake --build build && ./build/apps/demo-glfw/dc_demo_glfw`
   opens a window and renders the D2.1 scene.
2. The frame loop sustains ≥ 60 fps on a default modern Linux laptop
   (verified by an FPS HUD, or absent that, by `--measure` flag
   printing avg ms/frame).
3. Pressing `R` reloads the scene without restart.
4. Pressing `Esc` exits cleanly (no leaked GL resources — verifiable
   via debug callback or just clean shutdown).
5. ROADMAP.md "Next Up" GLFW item is checked, with a one-line how-to
   pointer to the demo.

## Constraints
- **Performance:** must hit 60 fps for the smoke scene. The smoke
  scene is small enough that anything less indicates a real bug.
- **Compatibility:** GLFW is a new dep. Vendor under `third_party/`
  alongside GLAD; no system install required.
- **Dependencies:** none. (CMake `find_package(glfw3)` if available;
  fallback to vendored.)

## Affected systems
- new `apps/demo-glfw/`
- new `core/include/dc/gl/GlfwGlContext.hpp` + `core/src/gl/GlfwGlContext.cpp`
- `core/CMakeLists.txt` (optional GLFW backend, gated by `DC_BUILD_GLFW`)
- `third_party/glfw/` (vendored)
- `ROADMAP.md`

## Alternatives considered
- **SDL3 instead of GLFW** — equivalent capability, larger dep
  footprint. GLFW preferred for minimalism.
- **Build the embedding example first** — rejected, the embedding
  example needs a windowed host to embed *into*.

## Risks
- GLFW's threading model differs subtly from OSMesa's; the existing
  `Renderer` may make implicit-main-thread assumptions that break
  here. Mitigation: keep all GL calls on the main thread in the demo;
  if `Renderer` blows up on that, fix in a follow-up.

## Open questions
- Whether to vendor GLFW or rely on system find_package. Default:
  vendor for offline-build parity.
