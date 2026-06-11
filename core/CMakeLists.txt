cmake_minimum_required(VERSION 3.20)
project(dc_core LANGUAGES C CXX)

# ---- Options ----
option(DC_BUILD_TESTS "Build core tests in core/tests/" ON)
option(DC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(DC_HAS_WEBSOCKET "Build WebSocket data source (requires easywsclient)" OFF)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Third-party root
# Override with: -DTHIRD_PARTY_ROOT=C:/wherever
if (NOT DEFINED THIRD_PARTY_ROOT)
  if (WIN32)
    set(THIRD_PARTY_ROOT "C:/thirdparty")
  else()
    set(THIRD_PARTY_ROOT "${CMAKE_SOURCE_DIR}/third_party")
  endif()
endif()

# ---- Threads
find_package(Threads REQUIRED)

# =======================
# RapidJSON (header-only)
# =======================
# We want #include <rapidjson/document.h>
# Accept both:
#   C:/thirdparty/rapidjson            (contains folder 'rapidjson/')
#   C:/thirdparty/rapidjson/include    (contains folder 'rapidjson/')
set(RAPIDJSON_HINTS
  "${RAPIDJSON_ROOT}"
  "$ENV{RAPIDJSON_ROOT}"
  "${THIRD_PARTY_ROOT}/rapidjson"
  "${THIRD_PARTY_ROOT}/rapidjson/include"
)

find_path(RAPIDJSON_INCLUDE_DIR
  NAMES rapidjson/document.h
  HINTS ${RAPIDJSON_HINTS}
)

if (NOT RAPIDJSON_INCLUDE_DIR)
  message(FATAL_ERROR
    "RapidJSON headers not found. Set RAPIDJSON_ROOT or place them under "
    "${THIRD_PARTY_ROOT}/rapidjson so that <rapidjson/document.h> exists.")
endif()

add_library(RapidJSON::rapidjson INTERFACE IMPORTED)
target_include_directories(RapidJSON::rapidjson INTERFACE "${RAPIDJSON_INCLUDE_DIR}")
message(STATUS "RapidJSON include dir: ${RAPIDJSON_INCLUDE_DIR}")

# =======================
# Core library (no graphics-API deps)
# =======================
file(GLOB_RECURSE DC_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# Exclude gpu/ sources — those belong to dc_gpu (WebGPU/Dawn backend).
# (ENC-501: the GL backend dc_gl + core/src/gl/ were deleted; nothing to filter.)
list(FILTER DC_SOURCES EXCLUDE REGEX ".*/gpu/.*")
# Exclude host/ sources — standalone executables.
list(FILTER DC_SOURCES EXCLUDE REGEX ".*/host/.*")
# Exclude WebSocketDataSource — belongs to dc_ws (conditional).
list(FILTER DC_SOURCES EXCLUDE REGEX ".*WebSocketDataSource\\.cpp$")

file(GLOB_RECURSE DC_PUBLIC_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(dc STATIC ${DC_SOURCES} ${DC_PUBLIC_HEADERS})
target_include_directories(dc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(dc PRIVATE "${THIRD_PARTY_ROOT}/stb")
target_link_libraries(dc PUBLIC Threads::Threads RapidJSON::rapidjson)

# Warnings
if (MSVC)
  target_compile_options(dc PRIVATE /W4)
  if (DC_WARNINGS_AS_ERRORS)
    target_compile_options(dc PRIVATE /WX)
  endif()
else()
  target_compile_options(dc PRIVATE -Wall -Wextra -Wpedantic)
  if (DC_WARNINGS_AS_ERRORS)
    target_compile_options(dc PRIVATE -Werror)
  endif()
endif()

# =======================
# (ENC-501) GL backend removed.
# =======================
# The OpenGL rendering backend (dc_gl) and its dependencies — GLAD (GL loader),
# OSMesa (headless GL), and GLFW (windowed GL) — were deleted in the WebGPU/Dawn
# migration. dc_gpu (Dawn) is now THE renderer. The default build (no Dawn) is
# pure `dc` + the logic tests and needs none of these; the Dawn build adds dc_gpu.
# Windowed/interactive on-screen demos are deferred to ENC-497 (Dawn surface).

# =======================
# dc_ws library (WebSocket data source, conditional)
# =======================
set(EASYWSCLIENT_DIR "${THIRD_PARTY_ROOT}/easywsclient")
if (DC_HAS_WEBSOCKET AND EXISTS "${EASYWSCLIENT_DIR}/easywsclient.cpp")
  add_library(dc_ws STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/data/WebSocketDataSource.cpp"
    "${EASYWSCLIENT_DIR}/easywsclient.cpp"
  )
  target_include_directories(dc_ws PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${THIRD_PARTY_ROOT}"
  )
  target_link_libraries(dc_ws PUBLIC dc)
  target_compile_definitions(dc_ws PUBLIC DC_HAS_WEBSOCKET=1)
  if (NOT MSVC)
    target_compile_options(dc_ws PRIVATE -Wall -Wextra -Wno-sign-compare)
  endif()
  message(STATUS "dc_ws (WebSocket) enabled")
else()
  if (DC_HAS_WEBSOCKET)
    message(STATUS "DC_HAS_WEBSOCKET=ON but easywsclient not found at ${EASYWSCLIENT_DIR}")
  else()
    message(STATUS "dc_ws disabled (DC_HAS_WEBSOCKET=OFF)")
  endif()
endif()

# =======================
# Headless render demos — migrated to Dawn (ENC-501).
# =======================
# The showcase / live / dashboard servers and the gallery PNG demo render scenes
# headless via DawnSceneRenderer + DawnDevice readback (mirroring the JsonHost
# Dawn path). They are built only when Dawn is available — see the
# "Headless demo servers (Dawn)" block after the dc_gpu definition below.
#
# REMOVED demos: hello_glfw and the windowed/interactive demos (d2_7_candle_demo,
# d3_5_recipe_demo, d4_6_full_demo, d5_7_interactive_demo, d6_3_live_demo) needed
# an on-screen GL/GLFW surface. ENC-497 lands the Dawn on-screen surface: the
# canonical replacement for hello_glfw is dc_dawn_window_demo (built with
# -DDC_DAWN_WINDOWED=ON; see the DawnWindowContext block after dc_gpu). The OSMesa-
# only headless showcase demos (d76_showcase, d77_declarative, d78_showcase) were
# also removed — their coverage is carried by the migrated servers + the Dawn
# render/golden tests.

# =======================
# Dawn (WebGPU native backend) — build-from-source via FetchContent
# =======================
# ENC-479 (P0.1): optional WebGPU/Dawn backend for the renderer migration.
#
# OFF by default so normal dc / dc_gl builds and other tickets' CI are
# unaffected. Turn ON with -DDC_FETCH_DAWN=ON to fetch and build Dawn from
# source. This mirrors the graceful-skip pattern used by the OSMesa/GLFW
# blocks above: when Dawn is unavailable we set DC_HAS_DAWN=FALSE and emit a
# clean STATUS skip, and dc / dc_gl build unchanged.
#
# Pinned Dawn revision (do NOT track a moving branch):
#   commit 58263faefe3c52fac4656825c6d55f85ee3c7536
#   == tip of branch chromium/7880 as of 2026-06-09.
# This is an explicit immutable commit hash so the build is reproducible.
# The full Dawn build is slow (30-60+ min first build, needs python3 + ninja);
# it is validated in CI (ENC-499), not necessarily locally.
option(DC_FETCH_DAWN "Fetch and build Dawn (WebGPU) from source for dc_gpu" OFF)

# ENC-497 (P4.1) — windowed (on-screen) presentation build path.
#
# OFF by default so the headless build stays lean (the 169 tests + WASM/browser
# stuff only need the offscreen path). When ON, the build:
#   * re-enables Dawn's X11 Vulkan SURFACE support (DAWN_USE_X11=ON below) so a
#     wgpu::Surface can be created from an Xlib window — Wayland and Dawn's own
#     bundled GLFW stay OFF (we use the SYSTEM glfw for the window, and force the
#     X11/XWayland backend at runtime, matching how Chrome runs WebGPU here),
#   * links the system GLFW into dc_gpu + compiles DawnWindowContext.cpp,
#   * builds the dawn_window_demo (the canonical embedding template that replaces
#     the deleted GL hello_glfw).
# Requires -DDC_FETCH_DAWN=ON (it extends the Dawn render path).
option(DC_DAWN_WINDOWED "Build the on-screen windowed Dawn surface path (ENC-497)" OFF)

set(DC_HAS_DAWN FALSE)
if (DC_FETCH_DAWN)
  include(FetchContent)

  set(DC_DAWN_GIT_REPOSITORY "https://dawn.googlesource.com/dawn")
  # Pinned immutable commit (tip of chromium/7880 @ 2026-06-09). See note above.
  set(DC_DAWN_GIT_TAG "58263faefe3c52fac4656825c6d55f85ee3c7536")

  # --- Keep Dawn's build lean for our use ---
  # Fetch Dawn's own deps with its python helper instead of depot_tools.
  set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
  # Disable Dawn samples / tests / benchmarks / fuzzers / node bindings.
  set(DAWN_BUILD_SAMPLES        OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_BENCHMARKS     OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_FUZZERS        OFF CACHE BOOL "" FORCE)
  set(DAWN_BUILD_NODE_BINDINGS  OFF CACHE BOOL "" FORCE)
  set(DAWN_ENABLE_INSTALL       OFF CACHE BOOL "" FORCE)
  # Don't build Dawn's bundled GLFW windowing utils or Wayland surface support.
  # We don't need them (samples are OFF; dc_gpu is headless for now, and our own
  # windowing later uses the system GLFW, not Dawn's). Building Dawn's GLFW pulls
  # in a wayland-scanner requirement that fails on bare Linux CI runners.
  set(DAWN_USE_GLFW             OFF CACHE BOOL "" FORCE)
  set(DAWN_USE_WAYLAND          OFF CACHE BOOL "" FORCE)
  # X11 surface support. The HEADLESS build (offscreen render + readback) needs no
  # windowing system, and disabling X11 avoids Dawn's X11/Xlib-xcb.h build
  # requirement — so X11 is OFF by default. ENC-497: the WINDOWED build path
  # (DC_DAWN_WINDOWED=ON) re-enables the X11 Vulkan surface (Xlib) so a
  # wgpu::Surface can be created from a GLFW X11 window. Wayland + Dawn's bundled
  # GLFW stay OFF either way (we use the SYSTEM glfw and force X11/XWayland).
  if (DC_DAWN_WINDOWED)
    set(DAWN_USE_X11            ON  CACHE BOOL "" FORCE)
    message(STATUS "DC_DAWN_WINDOWED=ON: re-enabling Dawn X11 Vulkan surface (DAWN_USE_X11=ON)")
  else()
    set(DAWN_USE_X11            OFF CACHE BOOL "" FORCE)
  endif()
  # Don't build Dawn's experimental C++20-modules bindings (dawncpp_module).
  # Dawn's own probe is a false positive on GCC 13.x (its `-fmodules-ts` test
  # compiles, so DAWN_SUPPORTS_CXX_MODULES=True), but CMake's Ninja generator
  # can't do GCC C++-module dependency scanning → "compiler does not provide a
  # way to discover the import graph" at generate time. We don't need the module
  # target (nothing links it when off — see Dawn src/dawn/CMakeLists.txt). Pre-
  # define the cache var FALSE so Dawn's check_cxx_source_compiles is skipped.
  set(DAWN_SUPPORTS_CXX_MODULES FALSE CACHE BOOL "" FORCE)
  # Tint: no standalone command-line tools or tests (only what Dawn links).
  set(TINT_BUILD_CMD_TOOLS      OFF CACHE BOOL "" FORCE)
  set(TINT_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
  # Build the single monolithic webgpu_dawn static library (default is STATIC,
  # set explicitly for clarity). Exposes target dawn::webgpu_dawn.
  set(DAWN_BUILD_MONOLITHIC_LIBRARY "STATIC" CACHE STRING "" FORCE)

  message(STATUS "DC_FETCH_DAWN=ON: fetching Dawn ${DC_DAWN_GIT_TAG} from ${DC_DAWN_GIT_REPOSITORY}")
  message(STATUS "  (build-from-source — first build is slow; validated in CI/ENC-499)")

  FetchContent_Declare(
    dawn
    GIT_REPOSITORY "${DC_DAWN_GIT_REPOSITORY}"
    GIT_TAG        "${DC_DAWN_GIT_TAG}"
    GIT_SHALLOW    FALSE   # need the pinned commit; shallow can't fetch arbitrary SHA
    # Do NOT recursively clone Dawn's git submodules (the full chromium tree:
    # build/, buildtools/, tools/clang, etc.). That clone is enormous and one of
    # those chromium submodules fails to update on CI runners ("Failed to update
    # submodules"). Instead, GIT_SUBMODULES "" fetches none, and Dawn's own
    # DAWN_FETCH_DEPENDENCIES=ON (above) pulls only the curated deps it needs via
    # fetch_dawn_dependencies.py. (Empty string == no submodules on CMake >= 3.16.)
    GIT_SUBMODULES ""
  )
  # Dawn requires C++20 (its dawncpp_module target uses C++ modules). Our `dc` /
  # `dc_gl` targets are already defined above with C++17 (the standard property is
  # captured per-target at add_library time), so raising the default here only
  # affects Dawn's targets and `dc_gpu` below — not the core libs. Restore after.
  set(_dc_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  FetchContent_MakeAvailable(dawn)
  set(CMAKE_CXX_STANDARD "${_dc_saved_cxx_standard}")

  if (TARGET dawn::webgpu_dawn OR TARGET webgpu_dawn)
    set(DC_HAS_DAWN TRUE)
    message(STATUS "Dawn available — dc_gpu will be built")
  else()
    set(DC_HAS_DAWN FALSE)
    message(STATUS "Dawn fetched but target dawn::webgpu_dawn not found — dc_gpu skipped")
  endif()
else()
  message(STATUS "Dawn not enabled (DC_FETCH_DAWN=OFF) — dc_gpu will not be built")
endif()

# =======================
# dc_gpu library (WebGPU/Dawn rendering backend) — scaffold
# =======================
# ENC-479 (P0.1): an (almost) empty static library so future WebGPU backend
# work has a real, linkable target. Built only when Dawn is available.
if (DC_HAS_DAWN)
  # All gpu/ sources are globbed in (placeholder.cpp + ENC-480 DawnDevice.cpp +
  # future Dawn backend sources).
  file(GLOB DC_GPU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/*.cpp")
  # ENC-497: DawnWindowContext.cpp (the on-screen surface/swapchain) pulls in GLFW
  # + X11 headers, so it is compiled ONLY in the windowed build (added back below
  # with the GLFW link). Exclude it from the default headless glob so the lean
  # build (and the browser/WASM globs that reuse src/gpu/*.cpp) never see it.
  list(FILTER DC_GPU_SOURCES EXCLUDE REGEX ".*/DawnWindowContext\\.cpp$")
  add_library(dc_gpu STATIC ${DC_GPU_SOURCES})
  target_include_directories(dc_gpu PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  # Dawn's public C++ wrapper (webgpu_cpp.h) requires C++20 (it uses `requires`
  # expressions and concepts in its callback helpers). dc / dc_gl stay C++17 —
  # only dc_gpu and anything including the Dawn headers (the ENC-480 test) need
  # C++20. PUBLIC so consumers (the test) inherit it.
  target_compile_features(dc_gpu PUBLIC cxx_std_20)
  # Link the Dawn monolithic WebGPU target. Prefer the namespaced alias.
  if (TARGET dawn::webgpu_dawn)
    target_link_libraries(dc_gpu PUBLIC dc dawn::webgpu_dawn)
  else()
    target_link_libraries(dc_gpu PUBLIC dc webgpu_dawn)
  endif()
  # ENC-485: dc_gpu no longer depends on the GL/GLAD header. The backend-agnostic
  # CPU-side buffer manager moved to dc::CpuBufferStore (dc/render/, pure dc), so
  # DawnTriSolidBackend reads CPU bytes through that instead of the GL
  # GpuBufferManager (which used to drag in <glad/gl.h>). No glad link here.

  if (NOT MSVC)
    target_compile_options(dc_gpu PRIVATE -Wall -Wextra -Wpedantic)
    if (DC_WARNINGS_AS_ERRORS)
      target_compile_options(dc_gpu PRIVATE -Werror)
    endif()
  endif()

  # ENC-497 (P4.1) — windowed (on-screen) presentation. When DC_DAWN_WINDOWED is
  # ON, add the DawnWindowContext TU (the wgpu::Surface/swapchain + GLFW X11 window
  # + per-frame blit-to-swapchain) to dc_gpu, link the SYSTEM GLFW, and define
  # DC_DAWN_WINDOWED so the header/TU compile their GLFW/X11 includes. The X11
  # Vulkan surface itself was re-enabled in Dawn above (DAWN_USE_X11=ON under the
  # same option). The headless build never enters this branch — the offscreen path
  # (and the 169 tests / WASM targets) is untouched.
  if (DC_DAWN_WINDOWED)
    # Prefer the CMake config package; fall back to locating the lib + headers.
    find_package(glfw3 QUIET)
    if (TARGET glfw)
      set(DC_GLFW_TARGET glfw)
      message(STATUS "DC_DAWN_WINDOWED: using glfw3 CMake package")
    else()
      find_library(DC_GLFW_LIB NAMES glfw glfw3)
      find_path(DC_GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)
      if (NOT DC_GLFW_LIB OR NOT DC_GLFW_INCLUDE_DIR)
        message(FATAL_ERROR
          "DC_DAWN_WINDOWED=ON but the system GLFW (libglfw + GLFW/glfw3.h) was not "
          "found. Install glfw (e.g. glfw-devel) or set CMAKE_PREFIX_PATH.")
      endif()
      add_library(dc_glfw_system UNKNOWN IMPORTED)
      set_target_properties(dc_glfw_system PROPERTIES
        IMPORTED_LOCATION "${DC_GLFW_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DC_GLFW_INCLUDE_DIR}")
      set(DC_GLFW_TARGET dc_glfw_system)
      message(STATUS "DC_DAWN_WINDOWED: using system GLFW lib=${DC_GLFW_LIB} include=${DC_GLFW_INCLUDE_DIR}")
    endif()

    target_sources(dc_gpu PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/DawnWindowContext.cpp")
    target_link_libraries(dc_gpu PUBLIC ${DC_GLFW_TARGET})
    # PUBLIC so the demo (and any embedder) that includes DawnWindowContext.hpp +
    # the GLFW native header sees the flag and the X11 expose macros line up.
    target_compile_definitions(dc_gpu PUBLIC DC_DAWN_WINDOWED=1)
    message(STATUS "dc_gpu: windowed Dawn surface path enabled (DC_DAWN_WINDOWED)")
  endif()

  message(STATUS "dc_gpu (WebGPU/Dawn) enabled")
else()
  message(STATUS "Skipping dc_gpu (requires DC_FETCH_DAWN=ON + Dawn available)")
endif()

# =======================
# dawn_window_demo (ENC-497) — on-screen windowed Dawn demo
# =======================
# The canonical embedding template that replaces the deleted GL hello_glfw: opens
# a GLFW X11 window, builds the d50 multi-pipeline scene, and runs the present loop
# (render -> blit to swapchain -> Present). Also reads the rendered scene back to a
# PNG for headless proof of the windowed render pipeline. Built only in the
# windowed Dawn build.
if (DC_HAS_DAWN AND DC_DAWN_WINDOWED)
  add_executable(dc_dawn_window_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/demos/dawn_window_demo.cpp")
  target_link_libraries(dc_dawn_window_demo PRIVATE dc_gpu)
  message(STATUS "dc_dawn_window_demo (ENC-497 windowed demo) enabled")
endif()

# =======================
# dc_json_host (ENC-501) — Dawn-only
# =======================
# The generic JSON chart host. Its render + readback path is Dawn (DawnDevice +
# CpuBufferStore + DawnSceneRenderer). ENC-501 removed the GL fallback, so the
# host now REQUIRES Dawn and is built only when DC_HAS_DAWN.
if (DC_HAS_DAWN)
  add_executable(dc_json_host "${CMAKE_CURRENT_SOURCE_DIR}/src/host/JsonHost.cpp")
  # dc_gpu carries the Dawn render path (DawnSceneRenderer, CpuBufferStore;
  # ChartSnapshot lives in dc). No GL is linked.
  target_link_libraries(dc_json_host PRIVATE dc_gpu)
  target_compile_definitions(dc_json_host PRIVATE DC_HAS_DAWN=1)
  message(STATUS "dc_json_host: Dawn render path")
else()
  message(STATUS "Skipping dc_json_host (requires DC_FETCH_DAWN=ON + Dawn available)")
endif()

# =======================
# Headless demo servers (Dawn) — ENC-501
# =======================
# The showcase / live / dashboard render servers and the gallery PNG demo were
# migrated from the GL path to DawnSceneRenderer + DawnDevice readback. They are
# built only when Dawn is available. Each links dc_gpu (which links dc).
if (DC_HAS_DAWN)
  # Showcase server (multi-chart demo: line + pie + table)
  add_executable(dc_showcase_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/showcase_server.cpp")
  target_link_libraries(dc_showcase_server PRIVATE dc_gpu)

  # Live streaming server (candle + volume panes for the web viewer)
  add_executable(dc_live_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/live_server.cpp")
  target_link_libraries(dc_live_server PRIVATE dc_gpu)

  # Dashboard server (multi-pane market command center)
  add_executable(dc_dashboard_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/dashboard_server.cpp")
  target_link_libraries(dc_dashboard_server PRIVATE dc_gpu)

  # Gallery demo (renders showcase PPMs)
  add_executable(dc_gallery "${CMAKE_CURRENT_SOURCE_DIR}/demos/gallery.cpp")
  target_link_libraries(dc_gallery PRIVATE dc_gpu)
else()
  message(STATUS "Skipping headless demo servers (require DC_FETCH_DAWN=ON)")
endif()

# =======================
# Tests
# =======================
if (DC_BUILD_TESTS)
  enable_testing()

  add_executable(dc_d1_1_smoke
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d1_1_smoke.cpp"
  )
  target_link_libraries(dc_d1_1_smoke PRIVATE dc)

  add_test(NAME dc_d1_1_smoke COMMAND dc_d1_1_smoke)

  add_executable(dc_d1_3_atomic_frame
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d1_3_atomic_frame.cpp"
  )
  target_link_libraries(dc_d1_3_atomic_frame PRIVATE dc)
  add_test(NAME dc_d1_3_atomic_frame COMMAND dc_d1_3_atomic_frame)

  add_executable(dc_d2_5_cache
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_5_cache.cpp"
  )
  target_link_libraries(dc_d2_5_cache PRIVATE dc)
  add_test(NAME dc_d2_5_cache COMMAND dc_d2_5_cache)

  add_executable(dc_d3_4_recipes
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d3_4_recipes.cpp"
  )
  target_link_libraries(dc_d3_4_recipes PRIVATE dc)
  add_test(NAME dc_d3_4_recipes COMMAND dc_d3_4_recipes)

  # D4.2 SMA (pure C++)
  add_executable(dc_d4_2_sma
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_2_sma.cpp"
  )
  target_link_libraries(dc_d4_2_sma PRIVATE dc)
  add_test(NAME dc_d4_2_sma COMMAND dc_d4_2_sma)

  # D4.3 Axis (pure C++)
  add_executable(dc_d4_3_axis
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_3_axis.cpp"
  )
  target_link_libraries(dc_d4_3_axis PRIVATE dc)
  add_test(NAME dc_d4_3_axis COMMAND dc_d4_3_axis)

  # D4.4 Bollinger (pure C++)
  add_executable(dc_d4_4_bollinger
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_4_bollinger.cpp"
  )
  target_link_libraries(dc_d4_4_bollinger PRIVATE dc)
  add_test(NAME dc_d4_4_bollinger COMMAND dc_d4_4_bollinger)

  # D4.5 MACD (pure C++)
  add_executable(dc_d4_5_macd
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_5_macd.cpp"
  )
  target_link_libraries(dc_d4_5_macd PRIVATE dc)
  add_test(NAME dc_d4_5_macd COMMAND dc_d4_5_macd)

  # D5.1 Viewport (pure C++)
  add_executable(dc_d5_1_viewport
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d5_1_viewport.cpp"
  )
  target_link_libraries(dc_d5_1_viewport PRIVATE dc)
  add_test(NAME dc_d5_1_viewport COMMAND dc_d5_1_viewport)

  # D6.1 Live ingest (pure C++)
  add_executable(dc_d6_1_live_ingest
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d6_1_live_ingest.cpp"
  )
  target_link_libraries(dc_d6_1_live_ingest PRIVATE dc)
  add_test(NAME dc_d6_1_live_ingest COMMAND dc_d6_1_live_ingest)

  # D7.1 Subscriptions metadata (pure C++)
  add_executable(dc_d7_1_subscriptions
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d7_1_subscriptions.cpp"
  )
  target_link_libraries(dc_d7_1_subscriptions PRIVATE dc)
  add_test(NAME dc_d7_1_subscriptions COMMAND dc_d7_1_subscriptions)

  # D7.2 ChartSession lifecycle (pure C++)
  add_executable(dc_d7_2_session
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d7_2_session.cpp"
  )
  target_link_libraries(dc_d7_2_session PRIVATE dc)
  add_test(NAME dc_d7_2_session COMMAND dc_d7_2_session)

  # D3.2 glyph atlas test (needs font file)
  set(DC_TEST_FONT "${THIRD_PARTY_ROOT}/test_font.ttf")
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d3_2_glyph_atlas
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d3_2_glyph_atlas.cpp"
    )
    target_link_libraries(dc_d3_2_glyph_atlas PRIVATE dc)
    target_compile_definitions(dc_d3_2_glyph_atlas PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d3_2_glyph_atlas COMMAND dc_d3_2_glyph_atlas)

    # D5.3 Crosshair (pure C++, needs font)
    add_executable(dc_d5_3_crosshair
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d5_3_crosshair.cpp"
    )
    target_link_libraries(dc_d5_3_crosshair PRIVATE dc)
    target_compile_definitions(dc_d5_3_crosshair PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d5_3_crosshair COMMAND dc_d5_3_crosshair)
  endif()

  # (ENC-501) GL render tests removed — coverage is carried by the Dawn render
  # tests + the Dawn-golden parity tests below.

  # D8.1 Zoom metrics (pure C++)
  add_executable(dc_d8_1_zoom_metrics
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_1_zoom_metrics.cpp"
  )
  target_link_libraries(dc_d8_1_zoom_metrics PRIVATE dc)
  add_test(NAME dc_d8_1_zoom_metrics COMMAND dc_d8_1_zoom_metrics)

  # D8.2 Candle aggregator (pure C++)
  add_executable(dc_d8_2_aggregator
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_2_aggregator.cpp"
  )
  target_link_libraries(dc_d8_2_aggregator PRIVATE dc)
  add_test(NAME dc_d8_2_aggregator COMMAND dc_d8_2_aggregator)

  # D8.3 Resolution policy (pure C++)
  add_executable(dc_d8_3_resolution_policy
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_3_resolution_policy.cpp"
  )
  target_link_libraries(dc_d8_3_resolution_policy PRIVATE dc)
  add_test(NAME dc_d8_3_resolution_policy COMMAND dc_d8_3_resolution_policy)

  # D8.4 Aggregation manager (pure C++)
  add_executable(dc_d8_4_aggregation
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_4_aggregation.cpp"
  )
  target_link_libraries(dc_d8_4_aggregation PRIVATE dc)
  add_test(NAME dc_d8_4_aggregation COMMAND dc_d8_4_aggregation)

  # D8.5 ChartSession aggregation integration (pure C++)
  add_executable(dc_d8_5_session_agg
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_5_session_agg.cpp"
  )
  target_link_libraries(dc_d8_5_session_agg PRIVATE dc)
  add_test(NAME dc_d8_5_session_agg COMMAND dc_d8_5_session_agg)

  # D9.1 Pane region (pure C++)
  add_executable(dc_d9_1_pane_region
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_1_pane_region.cpp"
  )
  target_link_libraries(dc_d9_1_pane_region PRIVATE dc)
  add_test(NAME dc_d9_1_pane_region COMMAND dc_d9_1_pane_region)

  # D9.3 Layout manager (pure C++)
  add_executable(dc_d9_3_layout_manager
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_3_layout_manager.cpp"
  )
  target_link_libraries(dc_d9_3_layout_manager PRIVATE dc)
  add_test(NAME dc_d9_3_layout_manager COMMAND dc_d9_3_layout_manager)

  # D9.4 Layout interaction (pure C++)
  add_executable(dc_d9_4_layout_interaction
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_4_layout_interaction.cpp"
  )
  target_link_libraries(dc_d9_4_layout_interaction PRIVATE dc)
  add_test(NAME dc_d9_4_layout_interaction COMMAND dc_d9_4_layout_interaction)

  # D9.5 Multi-viewport session (pure C++)
  add_executable(dc_d9_5_multi_viewport
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_5_multi_viewport.cpp"
  )
  target_link_libraries(dc_d9_5_multi_viewport PRIVATE dc)
  add_test(NAME dc_d9_5_multi_viewport COMMAND dc_d9_5_multi_viewport)

  # D10.2 DataPicker (pure C++)
  add_executable(dc_d10_2_hit_test
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_2_hit_test.cpp"
  )
  target_link_libraries(dc_d10_2_hit_test PRIVATE dc)
  add_test(NAME dc_d10_2_hit_test COMMAND dc_d10_2_hit_test)

  # D10.3 Dynamic axis (pure C++)
  add_executable(dc_d10_3_dynamic_axis
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_3_dynamic_axis.cpp"
  )
  target_link_libraries(dc_d10_3_dynamic_axis PRIVATE dc)
  add_test(NAME dc_d10_3_dynamic_axis COMMAND dc_d10_3_dynamic_axis)

  # D11.1 Input click (pure C++)
  add_executable(dc_d11_1_input_click
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_1_input_click.cpp"
  )
  target_link_libraries(dc_d11_1_input_click PRIVATE dc)
  add_test(NAME dc_d11_1_input_click COMMAND dc_d11_1_input_click)

  # D11.2 Auto-scale (pure C++)
  add_executable(dc_d11_2_autoscale
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_2_autoscale.cpp"
  )
  target_link_libraries(dc_d11_2_autoscale PRIVATE dc)
  add_test(NAME dc_d11_2_autoscale COMMAND dc_d11_2_autoscale)

  # D11.3 Selection state (pure C++)
  add_executable(dc_d11_3_selection
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_3_selection.cpp"
  )
  target_link_libraries(dc_d11_3_selection PRIVATE dc)
  add_test(NAME dc_d11_3_selection COMMAND dc_d11_3_selection)

  # D11.4 Highlight recipe (pure C++)
  add_executable(dc_d11_4_highlight
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_4_highlight.cpp"
  )
  target_link_libraries(dc_d11_4_highlight PRIVATE dc)
  add_test(NAME dc_d11_4_highlight COMMAND dc_d11_4_highlight)

  # D11.5 Tooltip recipe (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d11_5_tooltip
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_5_tooltip.cpp"
    )
    target_link_libraries(dc_d11_5_tooltip PRIVATE dc)
    target_compile_definitions(dc_d11_5_tooltip PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d11_5_tooltip COMMAND dc_d11_5_tooltip)
  endif()

  # D11.6 Interaction GL integration test (needs font + GL)

  # D12.1 Grid lines (pure C++)
  add_executable(dc_d12_1_grid
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_1_grid.cpp"
  )
  target_link_libraries(dc_d12_1_grid PRIVATE dc)
  add_test(NAME dc_d12_1_grid COMMAND dc_d12_1_grid)

  # D12.2 AA ticks (pure C++)
  add_executable(dc_d12_2_aa_ticks
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_2_aa_ticks.cpp"
  )
  target_link_libraries(dc_d12_2_aa_ticks PRIVATE dc)
  add_test(NAME dc_d12_2_aa_ticks COMMAND dc_d12_2_aa_ticks)

  # D12.3 Data-value X-axis (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d12_3_data_xaxis
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_3_data_xaxis.cpp"
    )
    target_link_libraries(dc_d12_3_data_xaxis PRIVATE dc)
    target_compile_definitions(dc_d12_3_data_xaxis PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d12_3_data_xaxis COMMAND dc_d12_3_data_xaxis)
  endif()

  # D12.4 Axis styling (pure C++)
  add_executable(dc_d12_4_axis_style
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_4_axis_style.cpp"
  )
  target_link_libraries(dc_d12_4_axis_style PRIVATE dc)
  add_test(NAME dc_d12_4_axis_style COMMAND dc_d12_4_axis_style)

  # D12.5 Spine + label alignment (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d12_5_spine_labels
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_5_spine_labels.cpp"
    )
    target_link_libraries(dc_d12_5_spine_labels PRIVATE dc)
    target_compile_definitions(dc_d12_5_spine_labels PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d12_5_spine_labels COMMAND dc_d12_5_spine_labels)
  endif()

  # D12.6 Axis GL integration test (needs font + GL)

  # D13.1 NiceTimeTicks (pure C++)
  add_executable(dc_d13_1_time_ticks
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d13_1_time_ticks.cpp"
  )
  target_link_libraries(dc_d13_1_time_ticks PRIVATE dc)
  add_test(NAME dc_d13_1_time_ticks COMMAND dc_d13_1_time_ticks)

  # D13.2 TimeFormat (pure C++)
  add_executable(dc_d13_2_time_format
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d13_2_time_format.cpp"
  )
  target_link_libraries(dc_d13_2_time_format PRIVATE dc)
  add_test(NAME dc_d13_2_time_format COMMAND dc_d13_2_time_format)

  # D13.3 Time-axis integration (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d13_3_time_axis
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d13_3_time_axis.cpp"
    )
    target_link_libraries(dc_d13_3_time_axis PRIVATE dc)
    target_compile_definitions(dc_d13_3_time_axis PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d13_3_time_axis COMMAND dc_d13_3_time_axis)
  endif()

  # D13.4 FakeDataSource timestamps (pure C++)
  add_executable(dc_d13_4_fake_timestamps
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d13_4_fake_timestamps.cpp"
  )
  target_link_libraries(dc_d13_4_fake_timestamps PRIVATE dc)
  add_test(NAME dc_d13_4_fake_timestamps COMMAND dc_d13_4_fake_timestamps)

  # D13.5 Time-axis GL integration test (needs font + GL)

  # D14.1 VolumeRecipe (pure C++)
  add_executable(dc_d14_1_volume
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_1_volume.cpp"
  )
  target_link_libraries(dc_d14_1_volume PRIVATE dc)
  add_test(NAME dc_d14_1_volume COMMAND dc_d14_1_volume)

  # D14.2 DrawItem visibility (GL)

  # D14.3 SeriesInfo (pure C++)
  add_executable(dc_d14_3_series_info
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_3_series_info.cpp"
  )
  target_link_libraries(dc_d14_3_series_info PRIVATE dc)
  add_test(NAME dc_d14_3_series_info COMMAND dc_d14_3_series_info)

  # D14.4 LegendRecipe (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d14_4_legend
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_4_legend.cpp"
    )
    target_link_libraries(dc_d14_4_legend PRIVATE dc)
    target_compile_definitions(dc_d14_4_legend PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d14_4_legend COMMAND dc_d14_4_legend)
  endif()

  # D14.5 Legend hit test (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d14_5_legend_toggle
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_5_legend_toggle.cpp"
    )
    target_link_libraries(dc_d14_5_legend_toggle PRIVATE dc)
    target_compile_definitions(dc_d14_5_legend_toggle PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d14_5_legend_toggle COMMAND dc_d14_5_legend_toggle)
  endif()

  # D14.6 Volume GL integration test (needs font + GL)

  # D15.1 ZoomController (pure C++)
  add_executable(dc_d15_1_zoom_controller
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d15_1_zoom_controller.cpp"
  )
  target_link_libraries(dc_d15_1_zoom_controller PRIVATE dc)
  add_test(NAME dc_d15_1_zoom_controller COMMAND dc_d15_1_zoom_controller)

  # D15.4 ScrollIndicatorRecipe (pure C++)
  add_executable(dc_d15_4_scroll_indicator
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d15_4_scroll_indicator.cpp"
  )
  target_link_libraries(dc_d15_4_scroll_indicator PRIVATE dc)
  add_test(NAME dc_d15_4_scroll_indicator COMMAND dc_d15_4_scroll_indicator)

  # D16.1 DrawingStore (pure C++)
  add_executable(dc_d16_1_drawing_store
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d16_1_drawing_store.cpp"
  )
  target_link_libraries(dc_d16_1_drawing_store PRIVATE dc)
  add_test(NAME dc_d16_1_drawing_store COMMAND dc_d16_1_drawing_store)

  # D16.4 DrawingRecipe (pure C++)
  add_executable(dc_d16_4_drawing_recipe
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d16_4_drawing_recipe.cpp"
  )
  target_link_libraries(dc_d16_4_drawing_recipe PRIVATE dc)
  add_test(NAME dc_d16_4_drawing_recipe COMMAND dc_d16_4_drawing_recipe)

  # D16.5 DrawingInteraction (pure C++)
  add_executable(dc_d16_5_drawing_interaction
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d16_5_drawing_interaction.cpp"
  )
  target_link_libraries(dc_d16_5_drawing_interaction PRIVATE dc)
  add_test(NAME dc_d16_5_drawing_interaction COMMAND dc_d16_5_drawing_interaction)

  # D17.1 Indicators (pure C++)
  add_executable(dc_d17_1_indicators
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d17_1_indicators.cpp"
  )
  target_link_libraries(dc_d17_1_indicators PRIVATE dc)
  add_test(NAME dc_d17_1_indicators COMMAND dc_d17_1_indicators)

  # D17.2 RSI Recipe (pure C++)
  add_executable(dc_d17_2_rsi_recipe
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d17_2_rsi_recipe.cpp"
  )
  target_link_libraries(dc_d17_2_rsi_recipe PRIVATE dc)
  add_test(NAME dc_d17_2_rsi_recipe COMMAND dc_d17_2_rsi_recipe)

  # D17.4 Stochastic Recipe (pure C++)
  add_executable(dc_d17_4_stochastic_recipe
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d17_4_stochastic_recipe.cpp"
  )
  target_link_libraries(dc_d17_4_stochastic_recipe PRIVATE dc)
  add_test(NAME dc_d17_4_stochastic_recipe COMMAND dc_d17_4_stochastic_recipe)

  # D19.1 Undo/Redo (pure C++)
  add_executable(dc_d19_1_undo_redo
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d19_1_undo_redo.cpp"
  )
  target_link_libraries(dc_d19_1_undo_redo PRIVATE dc)
  add_test(NAME dc_d19_1_undo_redo COMMAND dc_d19_1_undo_redo)

  # D19.2 Drawing serialization (pure C++)
  add_executable(dc_d19_2_drawing_serial
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d19_2_drawing_serial.cpp"
  )
  target_link_libraries(dc_d19_2_drawing_serial PRIVATE dc)
  add_test(NAME dc_d19_2_drawing_serial COMMAND dc_d19_2_drawing_serial)

  # D20.1 Theme (pure C++)
  add_executable(dc_d20_1_theme
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d20_1_theme.cpp"
  )
  target_link_libraries(dc_d20_1_theme PRIVATE dc)
  add_test(NAME dc_d20_1_theme COMMAND dc_d20_1_theme)

  # D21.1 Extended drawings (pure C++)
  add_executable(dc_d21_1_extended_drawings
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d21_1_extended_drawings.cpp"
  )
  target_link_libraries(dc_d21_1_extended_drawings PRIVATE dc)
  add_test(NAME dc_d21_1_extended_drawings COMMAND dc_d21_1_extended_drawings)

  # D22.1 Chart state save/load (pure C++)
  add_executable(dc_d22_1_chart_state
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d22_1_chart_state.cpp"
  )
  target_link_libraries(dc_d22_1_chart_state PRIVATE dc)
  add_test(NAME dc_d22_1_chart_state COMMAND dc_d22_1_chart_state)

  # D23.1 Measure tool (pure C++)
  add_executable(dc_d23_1_measure
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d23_1_measure.cpp"
  )
  target_link_libraries(dc_d23_1_measure PRIVATE dc)
  add_test(NAME dc_d23_1_measure COMMAND dc_d23_1_measure)

  # D24.1 Minimap (pure C++)
  add_executable(dc_d24_1_minimap
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d24_1_minimap.cpp"
  )
  target_link_libraries(dc_d24_1_minimap PRIVATE dc)
  add_test(NAME dc_d24_1_minimap COMMAND dc_d24_1_minimap)

  # D26.1 Index buffer (pure C++)
  add_executable(dc_d26_1_index_buffer
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d26_1_index_buffer.cpp"
  )
  target_link_libraries(dc_d26_1_index_buffer PRIVATE dc)
  add_test(NAME dc_d26_1_index_buffer COMMAND dc_d26_1_index_buffer)

  # D26.3 Cross-pane selection trigger (pure C++)
  add_executable(dc_d26_3_cross_pane
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d26_3_cross_pane.cpp"
  )
  target_link_libraries(dc_d26_3_cross_pane PRIVATE dc)
  add_test(NAME dc_d26_3_cross_pane COMMAND dc_d26_3_cross_pane)

  # D27.1 Easing functions (pure C++)
  add_executable(dc_d27_1_easing
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d27_1_easing.cpp"
  )
  target_link_libraries(dc_d27_1_easing PRIVATE dc)
  add_test(NAME dc_d27_1_easing COMMAND dc_d27_1_easing)

  # D27.2 Tween & AnimationManager (pure C++)
  add_executable(dc_d27_2_tween
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d27_2_tween.cpp"
  )
  target_link_libraries(dc_d27_2_tween PRIVATE dc)
  add_test(NAME dc_d27_2_tween COMMAND dc_d27_2_tween)

  # D27.3 ViewportAnimator (pure C++)
  add_executable(dc_d27_3_viewport_anim
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d27_3_viewport_anim.cpp"
  )
  target_link_libraries(dc_d27_3_viewport_anim PRIVATE dc)
  add_test(NAME dc_d27_3_viewport_anim COMMAND dc_d27_3_viewport_anim)

  # GL-dependent tests for D17, D18

  # D30.1 Sort pattern (pure C++)
  add_executable(dc_d30_1_sort_pattern
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d30_1_sort_pattern.cpp"
  )
  target_link_libraries(dc_d30_1_sort_pattern PRIVATE dc)
  add_test(NAME dc_d30_1_sort_pattern COMMAND dc_d30_1_sort_pattern)

  # D31.1 EventBus (pure C++)
  add_executable(dc_d31_1_event_bus
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d31_1_event_bus.cpp"
  )
  target_link_libraries(dc_d31_1_event_bus PRIVATE dc)
  add_test(NAME dc_d31_1_event_bus COMMAND dc_d31_1_event_bus)

  # D31.2 EventBus integration (pure C++)
  add_executable(dc_d31_2_event_integration
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d31_2_event_integration.cpp"
  )
  target_link_libraries(dc_d31_2_event_integration PRIVATE dc)
  add_test(NAME dc_d31_2_event_integration COMMAND dc_d31_2_event_integration)

  # D32.1 DerivedBuffer (pure C++)
  add_executable(dc_d32_1_derived_buffer
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d32_1_derived_buffer.cpp"
  )
  target_link_libraries(dc_d32_1_derived_buffer PRIVATE dc)
  add_test(NAME dc_d32_1_derived_buffer COMMAND dc_d32_1_derived_buffer)

  # D32.2 DerivedBuffer integration (pure C++)
  add_executable(dc_d32_2_derived_integration
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d32_2_derived_integration.cpp"
  )
  target_link_libraries(dc_d32_2_derived_integration PRIVATE dc)
  add_test(NAME dc_d32_2_derived_integration COMMAND dc_d32_2_derived_integration)

  # D33.1 BoxSelection (pure C++)
  add_executable(dc_d33_1_box_selection
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d33_1_box_selection.cpp"
  )
  target_link_libraries(dc_d33_1_box_selection PRIVATE dc)
  add_test(NAME dc_d33_1_box_selection COMMAND dc_d33_1_box_selection)

  # D34.1 CursorHint (pure C++)
  add_executable(dc_d34_1_cursor_hint
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d34_1_cursor_hint.cpp"
  )
  target_link_libraries(dc_d34_1_cursor_hint PRIVATE dc)
  add_test(NAME dc_d34_1_cursor_hint COMMAND dc_d34_1_cursor_hint)

  # D35.1 DragDrop (pure C++)
  add_executable(dc_d35_1_drag_drop
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d35_1_drag_drop.cpp"
  )
  target_link_libraries(dc_d35_1_drag_drop PRIVATE dc)
  add_test(NAME dc_d35_1_drag_drop COMMAND dc_d35_1_drag_drop)

  # D35.2 DragDrop threshold (pure C++)
  add_executable(dc_d35_2_drag_threshold
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d35_2_drag_threshold.cpp"
  )
  target_link_libraries(dc_d35_2_drag_threshold PRIVATE dc)
  add_test(NAME dc_d35_2_drag_threshold COMMAND dc_d35_2_drag_threshold)

  # D37.1 Anchor (pure C++)
  add_executable(dc_d37_1_anchor
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d37_1_anchor.cpp"
  )
  target_link_libraries(dc_d37_1_anchor PRIVATE dc)
  add_test(NAME dc_d37_1_anchor COMMAND dc_d37_1_anchor)

  # D38.1 VirtualRange (pure C++)
  add_executable(dc_d38_1_virtual_range
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d38_1_virtual_range.cpp"
  )
  target_link_libraries(dc_d38_1_virtual_range PRIVATE dc)
  add_test(NAME dc_d38_1_virtual_range COMMAND dc_d38_1_virtual_range)

  # D38.2 VirtualRange integration (pure C++)
  add_executable(dc_d38_2_virtual_integration
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d38_2_virtual_integration.cpp"
  )
  target_link_libraries(dc_d38_2_virtual_integration PRIVATE dc)
  add_test(NAME dc_d38_2_virtual_integration COMMAND dc_d38_2_virtual_integration)

  # D39.1 FocusManager (pure C++)
  add_executable(dc_d39_1_focus_manager
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d39_1_focus_manager.cpp"
  )
  target_link_libraries(dc_d39_1_focus_manager PRIVATE dc)
  add_test(NAME dc_d39_1_focus_manager COMMAND dc_d39_1_focus_manager)

  # D39.2 Keyboard navigation (pure C++)
  add_executable(dc_d39_2_keyboard_nav
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d39_2_keyboard_nav.cpp"
  )
  target_link_libraries(dc_d39_2_keyboard_nav PRIVATE dc)
  add_test(NAME dc_d39_2_keyboard_nav COMMAND dc_d39_2_keyboard_nav)

  # D40.1 Annotations (pure C++)
  add_executable(dc_d40_1_annotations
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d40_1_annotations.cpp"
  )
  target_link_libraries(dc_d40_1_annotations PRIVATE dc)
  add_test(NAME dc_d40_1_annotations COMMAND dc_d40_1_annotations)

  # D40.2 Annotation query (pure C++)
  add_executable(dc_d40_2_annotation_query
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d40_2_annotation_query.cpp"
  )
  target_link_libraries(dc_d40_2_annotation_query PRIVATE dc)
  add_test(NAME dc_d40_2_annotation_query COMMAND dc_d40_2_annotation_query)

  # D42.1 CommandProcessor event emission (pure C++)
  add_executable(dc_d42_1_cmd_events
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d42_1_cmd_events.cpp"
  )
  target_link_libraries(dc_d42_1_cmd_events PRIVATE dc)
  add_test(NAME dc_d42_1_cmd_events COMMAND dc_d42_1_cmd_events)

  # D43.1 InteractionCoordinator (pure C++)
  add_executable(dc_d43_1_coordinator
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d43_1_coordinator.cpp"
  )
  target_link_libraries(dc_d43_1_coordinator PRIVATE dc)
  add_test(NAME dc_d43_1_coordinator COMMAND dc_d43_1_coordinator)

  # D43.2 InteractionCoordinator events (pure C++)
  add_executable(dc_d43_2_coordinator_events
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d43_2_coordinator_events.cpp"
  )
  target_link_libraries(dc_d43_2_coordinator_events PRIVATE dc)
  add_test(NAME dc_d43_2_coordinator_events COMMAND dc_d43_2_coordinator_events)

  # D48.1 DrawItemAnimator color animation (pure C++)
  add_executable(dc_d48_1_color_anim
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d48_1_color_anim.cpp"
  )
  target_link_libraries(dc_d48_1_color_anim PRIVATE dc)
  add_test(NAME dc_d48_1_color_anim COMMAND dc_d48_1_color_anim)

  # D48.2 DrawItemAnimator position/transform animation (pure C++)
  add_executable(dc_d48_2_position_anim
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d48_2_position_anim.cpp"
  )
  target_link_libraries(dc_d48_2_position_anim PRIVATE dc)
  add_test(NAME dc_d48_2_position_anim COMMAND dc_d48_2_position_anim)

  # D50.1 ContextMenu (pure C++)
  add_executable(dc_d50_1_context_menu
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d50_1_context_menu.cpp"
  )
  target_link_libraries(dc_d50_1_context_menu PRIVATE dc)
  add_test(NAME dc_d50_1_context_menu COMMAND dc_d50_1_context_menu)

  # D45.1 SceneSerializer simple round-trip (pure C++)
  add_executable(dc_d45_1_scene_serial
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d45_1_scene_serial.cpp"
  )
  target_link_libraries(dc_d45_1_scene_serial PRIVATE dc)
  add_test(NAME dc_d45_1_scene_serial COMMAND dc_d45_1_scene_serial)

  # D45.2 SceneSerializer complex round-trip (pure C++)
  add_executable(dc_d45_2_serial_roundtrip
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d45_2_serial_roundtrip.cpp"
  )
  target_link_libraries(dc_d45_2_serial_roundtrip PRIVATE dc)
  add_test(NAME dc_d45_2_serial_roundtrip COMMAND dc_d45_2_serial_roundtrip)

  # D51.1 CollisionSolver (pure C++)
  add_executable(dc_d51_1_collision
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d51_1_collision.cpp"
  )
  target_link_libraries(dc_d51_1_collision PRIVATE dc)
  add_test(NAME dc_d51_1_collision COMMAND dc_d51_1_collision)

  # D51.2 CollisionSolver priority (pure C++)
  add_executable(dc_d51_2_priority
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d51_2_priority.cpp"
  )
  target_link_libraries(dc_d51_2_priority PRIVATE dc)
  add_test(NAME dc_d51_2_priority COMMAND dc_d51_2_priority)

  # D52.1 LodManager (pure C++)
  add_executable(dc_d52_1_lod
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d52_1_lod.cpp"
  )
  target_link_libraries(dc_d52_1_lod PRIVATE dc)
  add_test(NAME dc_d52_1_lod COMMAND dc_d52_1_lod)

  # D52.2 LodManager hysteresis (pure C++)
  add_executable(dc_d52_2_lod_transition
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d52_2_lod_transition.cpp"
  )
  target_link_libraries(dc_d52_2_lod_transition PRIVATE dc)
  add_test(NAME dc_d52_2_lod_transition COMMAND dc_d52_2_lod_transition)

  # D53.1 ValidationConfig strict mode (pure C++)
  add_executable(dc_d53_1_validation
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d53_1_validation.cpp"
  )
  target_link_libraries(dc_d53_1_validation PRIVATE dc)
  add_test(NAME dc_d53_1_validation COMMAND dc_d53_1_validation)

  # D53.2 ValidationConfig clamp mode (pure C++)
  add_executable(dc_d53_2_validation_clamp
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d53_2_validation_clamp.cpp"
  )
  target_link_libraries(dc_d53_2_validation_clamp PRIVATE dc)
  add_test(NAME dc_d53_2_validation_clamp COMMAND dc_d53_2_validation_clamp)

  # D55.1 FrameProfiler section timing (pure C++)
  add_executable(dc_d55_1_profiler
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d55_1_profiler.cpp"
  )
  target_link_libraries(dc_d55_1_profiler PRIVATE dc)
  add_test(NAME dc_d55_1_profiler COMMAND dc_d55_1_profiler)

  # D55.2 FrameProfiler history & ScopedProfile (pure C++)
  add_executable(dc_d55_2_profiler_history
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d55_2_profiler_history.cpp"
  )
  target_link_libraries(dc_d55_2_profiler_history PRIVATE dc)
  add_test(NAME dc_d55_2_profiler_history COMMAND dc_d55_2_profiler_history)

  # D46.1 Gradient fill command (pure C++)
  add_executable(dc_d46_1_gradient_fill
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d46_1_gradient_fill.cpp"
  )
  target_link_libraries(dc_d46_1_gradient_fill PRIVATE dc)
  add_test(NAME dc_d46_1_gradient_fill COMMAND dc_d46_1_gradient_fill)

  # D46.2 Radial gradient command (pure C++)
  add_executable(dc_d46_2_radial_gradient
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d46_2_radial_gradient.cpp"
  )
  target_link_libraries(dc_d46_2_radial_gradient PRIVATE dc)
  add_test(NAME dc_d46_2_radial_gradient COMMAND dc_d46_2_radial_gradient)

  # D49.1 GestureRecognizer pinch (pure C++)
  add_executable(dc_d49_1_gesture
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d49_1_gesture.cpp"
  )
  target_link_libraries(dc_d49_1_gesture PRIVATE dc)
  add_test(NAME dc_d49_1_gesture COMMAND dc_d49_1_gesture)

  # D49.2 GestureRecognizer two-finger pan (pure C++)
  add_executable(dc_d49_2_two_finger_pan
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d49_2_two_finger_pan.cpp"
  )
  target_link_libraries(dc_d49_2_two_finger_pan PRIVATE dc)
  add_test(NAME dc_d49_2_two_finger_pan COMMAND dc_d49_2_two_finger_pan)

  # D57.1 BackoffCalculator (pure C++)
  add_executable(dc_d57_1_backoff
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d57_1_backoff.cpp"
  )
  target_link_libraries(dc_d57_1_backoff PRIVATE dc)
  add_test(NAME dc_d57_1_backoff COMMAND dc_d57_1_backoff)

  # D57.2 MessageCodec (pure C++)
  add_executable(dc_d57_2_codec
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d57_2_codec.cpp"
  )
  target_link_libraries(dc_d57_2_codec PRIVATE dc)
  add_test(NAME dc_d57_2_codec COMMAND dc_d57_2_codec)

  # D58.1 ThemeManager (pure C++)
  add_executable(dc_d58_1_theme_manager
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d58_1_theme_manager.cpp"
  )
  target_link_libraries(dc_d58_1_theme_manager PRIVATE dc)
  add_test(NAME dc_d58_1_theme_manager COMMAND dc_d58_1_theme_manager)

  # D58.2 Theme interpolation (pure C++)
  add_executable(dc_d58_2_theme_interpolate
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d58_2_theme_interpolate.cpp"
  )
  target_link_libraries(dc_d58_2_theme_interpolate PRIVATE dc)
  add_test(NAME dc_d58_2_theme_interpolate COMMAND dc_d58_2_theme_interpolate)

  # D60.1 Custom pipeline registration (pure C++)
  add_executable(dc_d60_1_custom_pipeline
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d60_1_custom_pipeline.cpp"
  )
  target_link_libraries(dc_d60_1_custom_pipeline PRIVATE dc)
  add_test(NAME dc_d60_1_custom_pipeline COMMAND dc_d60_1_custom_pipeline)

  # D60.2 Custom pipeline render registration (pure C++)
  add_executable(dc_d60_2_custom_render
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d60_2_custom_render.cpp"
  )
  target_link_libraries(dc_d60_2_custom_render PRIVATE dc)
  add_test(NAME dc_d60_2_custom_render COMMAND dc_d60_2_custom_render)

  # D61.1 Undo command (pure C++)
  add_executable(dc_d61_1_undo_cmd
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d61_1_undo_cmd.cpp"
  )
  target_link_libraries(dc_d61_1_undo_cmd PRIVATE dc)
  add_test(NAME dc_d61_1_undo_cmd COMMAND dc_d61_1_undo_cmd)

  # D61.2 UndoGroup compound undo (pure C++)
  add_executable(dc_d61_2_undo_group
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d61_2_undo_group.cpp"
  )
  target_link_libraries(dc_d61_2_undo_group PRIVATE dc)
  add_test(NAME dc_d61_2_undo_group COMMAND dc_d61_2_undo_group)

  # D54.1 DebugOverlay (pure C++)
  add_executable(dc_d54_1_debug_overlay
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d54_1_debug_overlay.cpp"
  )
  target_link_libraries(dc_d54_1_debug_overlay PRIVATE dc)
  add_test(NAME dc_d54_1_debug_overlay COMMAND dc_d54_1_debug_overlay)

  # D59.1 AccessibilityBridge (pure C++)
  add_executable(dc_d59_1_a11y
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d59_1_a11y.cpp"
  )
  target_link_libraries(dc_d59_1_a11y PRIVATE dc)
  add_test(NAME dc_d59_1_a11y COMMAND dc_d59_1_a11y)

  # D59.2 AccessibilityBridge JSON (pure C++)
  add_executable(dc_d59_2_a11y_json
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d59_2_a11y_json.cpp"
  )
  target_link_libraries(dc_d59_2_a11y_json PRIVATE dc)
  add_test(NAME dc_d59_2_a11y_json COMMAND dc_d59_2_a11y_json)

  # D56.1 BatchBuilder grouping/ordering (pure C++). ENC-501: BatchBuilder is
  # backend-agnostic; its header moved from dc/gl/ to dc/scene/ and this test now
  # links plain dc. (The GL-render d56_2_batch_gl was removed.)
  add_executable(dc_d56_1_batching
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d56_1_batching.cpp"
  )
  target_link_libraries(dc_d56_1_batching PRIVATE dc)
  add_test(NAME dc_d56_1_batching COMMAND dc_d56_1_batching)

  # D62.1 DChartFile save (pure C++)
  add_executable(dc_d62_1_dchart_save
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d62_1_dchart_save.cpp"
  )
  target_link_libraries(dc_d62_1_dchart_save PRIVATE dc)
  add_test(NAME dc_d62_1_dchart_save COMMAND dc_d62_1_dchart_save)

  # D62.2 DChartFile roundtrip (pure C++)
  add_executable(dc_d62_2_dchart_roundtrip
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d62_2_dchart_roundtrip.cpp"
  )
  target_link_libraries(dc_d62_2_dchart_roundtrip PRIVATE dc)
  add_test(NAME dc_d62_2_dchart_roundtrip COMMAND dc_d62_2_dchart_roundtrip)

  # D65.1 FreehandCapture (pure C++)
  add_executable(dc_d65_1_freehand
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d65_1_freehand.cpp"
  )
  target_link_libraries(dc_d65_1_freehand PRIVATE dc)
  add_test(NAME dc_d65_1_freehand COMMAND dc_d65_1_freehand)

  # D65.2 Polyline simplify / StrokeStore (pure C++)
  add_executable(dc_d65_2_polyline_simplify
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d65_2_polyline_simplify.cpp"
  )
  target_link_libraries(dc_d65_2_polyline_simplify PRIVATE dc)
  add_test(NAME dc_d65_2_polyline_simplify COMMAND dc_d65_2_polyline_simplify)

  # D44.1 AnnotationRenderer (needs font)
  if (EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d44_1_annotation_render
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d44_1_annotation_render.cpp"
    )
    target_link_libraries(dc_d44_1_annotation_render PRIVATE dc)
    target_compile_definitions(dc_d44_1_annotation_render PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d44_1_annotation_render COMMAND dc_d44_1_annotation_render)
  endif()

  # D54.2 DebugOverlay GL (needs OSMesa)

  # D44.2 AnnotationRenderer GL (needs font + GL)

  # D72.1 SessionBridge basic pub/sub (pure C++)
  add_executable(dc_d72_1_bridge
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d72_1_bridge.cpp"
  )
  target_link_libraries(dc_d72_1_bridge PRIVATE dc)
  add_test(NAME dc_d72_1_bridge COMMAND dc_d72_1_bridge)

  # D72.2 SessionBridge multi-type & sync control (pure C++)
  add_executable(dc_d72_2_sync
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d72_2_sync.cpp"
  )
  target_link_libraries(dc_d72_2_sync PRIVATE dc)
  add_test(NAME dc_d72_2_sync COMMAND dc_d72_2_sync)

  # D63.1 Log scale mapping (pure C++)
  add_executable(dc_d63_1_log_scale
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d63_1_log_scale.cpp"
  )
  target_link_libraries(dc_d63_1_log_scale PRIVATE dc)
  add_test(NAME dc_d63_1_log_scale COMMAND dc_d63_1_log_scale)

  # D63.2 Percentage & indexed scale (pure C++)
  add_executable(dc_d63_2_percentage_scale
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d63_2_percentage_scale.cpp"
  )
  target_link_libraries(dc_d63_2_percentage_scale PRIVATE dc)
  add_test(NAME dc_d63_2_percentage_scale COMMAND dc_d63_2_percentage_scale)

  # D69.1 ExtDrawingInteraction: pitchfork/ray/arrow (pure C++)
  add_executable(dc_d69_1_pitchfork
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d69_1_pitchfork.cpp"
  )
  target_link_libraries(dc_d69_1_pitchfork PRIVATE dc)
  add_test(NAME dc_d69_1_pitchfork COMMAND dc_d69_1_pitchfork)

  # D69.2 ExtDrawingInteraction: polygon/polyline/remove/clear (pure C++)
  add_executable(dc_d69_2_polygon
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d69_2_polygon.cpp"
  )
  target_link_libraries(dc_d69_2_polygon PRIVATE dc)
  add_test(NAME dc_d69_2_polygon COMMAND dc_d69_2_polygon)

  # D68.1 TextEditState insert/delete/caret (pure C++)
  add_executable(dc_d68_1_text_edit
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d68_1_text_edit.cpp"
  )
  target_link_libraries(dc_d68_1_text_edit PRIVATE dc)
  add_test(NAME dc_d68_1_text_edit COMMAND dc_d68_1_text_edit)

  # D68.2 TextEditState selection/clipboard/word nav (pure C++)
  add_executable(dc_d68_2_text_selection
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d68_2_text_selection.cpp"
  )
  target_link_libraries(dc_d68_2_text_selection PRIVATE dc)
  add_test(NAME dc_d68_2_text_selection COMMAND dc_d68_2_text_selection)

  # D73.1 AlertManager crossing alerts (pure C++)
  add_executable(dc_d73_1_alert
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d73_1_alert.cpp"
  )
  target_link_libraries(dc_d73_1_alert PRIVATE dc)
  add_test(NAME dc_d73_1_alert COMMAND dc_d73_1_alert)

  # D73.2 AlertManager range/condition tests (pure C++)
  add_executable(dc_d73_2_condition
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d73_2_condition.cpp"
  )
  target_link_libraries(dc_d73_2_condition PRIVATE dc)
  add_test(NAME dc_d73_2_condition COMMAND dc_d73_2_condition)

  # D64.1 Bezier tessellation (pure C++)
  add_executable(dc_d64_1_bezier
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d64_1_bezier.cpp"
  )
  target_link_libraries(dc_d64_1_bezier PRIVATE dc)
  add_test(NAME dc_d64_1_bezier COMMAND dc_d64_1_bezier)

  # D64.2 Arc & ellipse tessellation (pure C++)
  add_executable(dc_d64_2_arc_ellipse
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d64_2_arc_ellipse.cpp"
  )
  target_link_libraries(dc_d64_2_arc_ellipse PRIVATE dc)
  add_test(NAME dc_d64_2_arc_ellipse COMMAND dc_d64_2_arc_ellipse)

  # D66.1 HandleSet (pure C++)
  add_executable(dc_d66_1_handles
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d66_1_handles.cpp"
  )
  target_link_libraries(dc_d66_1_handles PRIVATE dc)
  add_test(NAME dc_d66_1_handles COMMAND dc_d66_1_handles)

  # D66.2 Handle drag interaction (pure C++)
  add_executable(dc_d66_2_handle_drag
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d66_2_handle_drag.cpp"
  )
  target_link_libraries(dc_d66_2_handle_drag PRIVATE dc)
  add_test(NAME dc_d66_2_handle_drag COMMAND dc_d66_2_handle_drag)

  # D67.1 SnapManager (pure C++)
  add_executable(dc_d67_1_snap
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d67_1_snap.cpp"
  )
  target_link_libraries(dc_d67_1_snap PRIVATE dc)
  add_test(NAME dc_d67_1_snap COMMAND dc_d67_1_snap)

  # D67.2 Magnet mode snap (pure C++)
  add_executable(dc_d67_2_magnet
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d67_2_magnet.cpp"
  )
  target_link_libraries(dc_d67_2_magnet PRIVATE dc)
  add_test(NAME dc_d67_2_magnet COMMAND dc_d67_2_magnet)

  # D70.1 TemporalFilter replay (pure C++)
  add_executable(dc_d70_1_replay
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d70_1_replay.cpp"
  )
  target_link_libraries(dc_d70_1_replay PRIVATE dc)
  add_test(NAME dc_d70_1_replay COMMAND dc_d70_1_replay)

  # D70.2 TemporalFilter step mode (pure C++)
  add_executable(dc_d70_2_step
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d70_2_step.cpp"
  )
  target_link_libraries(dc_d70_2_step PRIVATE dc)
  add_test(NAME dc_d70_2_step COMMAND dc_d70_2_step)

  # D71.1 HoverManager (pure C++)
  add_executable(dc_d71_1_hover
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d71_1_hover.cpp"
  )
  target_link_libraries(dc_d71_1_hover PRIVATE dc)
  add_test(NAME dc_d71_1_hover COMMAND dc_d71_1_hover)

  # D71.2 Tooltip lifecycle (pure C++)
  add_executable(dc_d71_2_tooltip
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d71_2_tooltip.cpp"
  )
  target_link_libraries(dc_d71_2_tooltip PRIVATE dc)
  add_test(NAME dc_d71_2_tooltip COMMAND dc_d71_2_tooltip)

  # D74.1 LayoutGrid (pure C++)
  add_executable(dc_d74_1_grid
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d74_1_grid.cpp"
  )
  target_link_libraries(dc_d74_1_grid PRIVATE dc)
  add_test(NAME dc_d74_1_grid COMMAND dc_d74_1_grid)

  # D74.2 LayoutGrid resize (pure C++)
  add_executable(dc_d74_2_resize
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d74_2_resize.cpp"
  )
  target_link_libraries(dc_d74_2_resize PRIVATE dc)
  add_test(NAME dc_d74_2_resize COMMAND dc_d74_2_resize)

  # D75.1 SvgExporter basic (pure C++)
  add_executable(dc_d75_1_svg
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d75_1_svg.cpp"
  )
  target_link_libraries(dc_d75_1_svg PRIVATE dc)
  add_test(NAME dc_d75_1_svg COMMAND dc_d75_1_svg)

  # D75.2 SvgExporter styles (pure C++)
  add_executable(dc_d75_2_svg_styles
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d75_2_svg_styles.cpp"
  )
  target_link_libraries(dc_d75_2_svg_styles PRIVATE dc)
  add_test(NAME dc_d75_2_svg_styles COMMAND dc_d75_2_svg_styles)

  # D77.1 SceneDocument parse/serialize (pure C++)
  add_executable(dc_d77_1_scene_document
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d77_1_scene_document.cpp"
  )
  target_link_libraries(dc_d77_1_scene_document PRIVATE dc)
  add_test(NAME dc_d77_1_scene_document COMMAND dc_d77_1_scene_document)

  # D77.2 SceneReconciler diff tests (pure C++)
  add_executable(dc_d77_2_reconciler
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d77_2_reconciler.cpp"
  )
  target_link_libraries(dc_d77_2_reconciler PRIVATE dc)
  add_test(NAME dc_d77_2_reconciler COMMAND dc_d77_2_reconciler)

  # D78.1 Theme presets (pure C++)
  add_executable(dc_d78_1_theme_presets
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d78_1_theme_presets.cpp"
  )
  target_link_libraries(dc_d78_1_theme_presets PRIVATE dc)
  add_test(NAME dc_d78_1_theme_presets COMMAND dc_d78_1_theme_presets)

  # D78.4 Grid style (pure C++)
  add_executable(dc_d78_4_grid_style
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d78_4_grid_style.cpp"
  )
  target_link_libraries(dc_d78_4_grid_style PRIVATE dc)
  add_test(NAME dc_d78_4_grid_style COMMAND dc_d78_4_grid_style)

  # D79.1 JSON host parse/serialize round-trip (pure C++)
  add_executable(dc_d79_1_json_host
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d79_1_json_host.cpp"
  )
  target_link_libraries(dc_d79_1_json_host PRIVATE dc)
  add_test(NAME dc_d79_1_json_host COMMAND dc_d79_1_json_host)

  # D79.2 JSON host GL integration (needs OSMesa)

  # D80.1 Binding document parse/serialize round-trip
  add_executable(dc_d80_1_binding_document
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d80_1_binding_document.cpp"
  )
  target_link_libraries(dc_d80_1_binding_document PRIVATE dc)
  add_test(NAME dc_d80_1_binding_document COMMAND dc_d80_1_binding_document)

  # D80.2 BindingEvaluator integration
  add_executable(dc_d80_2_binding_evaluator
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d80_2_binding_evaluator.cpp"
  )
  target_link_libraries(dc_d80_2_binding_evaluator PRIVATE dc)
  add_test(NAME dc_d80_2_binding_evaluator COMMAND dc_d80_2_binding_evaluator)

  # D80.3 Binding GL integration (needs OSMesa)

  # D81.1 Ingest backpressure (drop counter + overflow callback)
  add_executable(dc_d81_1_backpressure
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d81_1_backpressure.cpp"
  )
  target_link_libraries(dc_d81_1_backpressure PRIVATE dc)
  add_test(NAME dc_d81_1_backpressure COMMAND dc_d81_1_backpressure)

  # D81.2 CompositeDataSource (multi-source round-robin)
  add_executable(dc_d81_2_composite_source
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d81_2_composite_source.cpp"
  )
  target_link_libraries(dc_d81_2_composite_source PRIVATE dc)
  add_test(NAME dc_d81_2_composite_source COMMAND dc_d81_2_composite_source)

  # D81.3 Partial GPU uploads (coalesced dirty ranges)

  # ENC-480 (P0.2) Headless Dawn device: clear an offscreen target to a known
  # color and read the center pixel back synchronously. Guarded by DC_HAS_DAWN
  # like dc_gpu itself. On this headless box the test may need the lavapipe
  # software Vulkan ICD: set
  #   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
  if (DC_HAS_DAWN)
    add_executable(dc_d2_0_dawn_clear
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_0_dawn_clear.cpp"
    )
    target_link_libraries(dc_d2_0_dawn_clear PRIVATE dc_gpu)
    add_test(NAME dc_d2_0_dawn_clear COMMAND dc_d2_0_dawn_clear)

    # ENC-484 (P2.1) — triSolid@1 end-to-end on Dawn (D2.1 first render). Renders
    # a colored-triangle scene through the backend registry with DeviceKind::Dawn
    # into the headless DawnDevice offscreen target, reads it back, and asserts the
    # interior == fill color and a clear region == clear color. ENC-501: the
    # CPU-side vertex bytes now come from dc::CpuBufferStore (pure dc), so this
    # links only dc_gpu (dc_gl is gone).
    add_executable(dc_d2_1_dawn_first_render
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_1_dawn_first_render.cpp"
    )
    target_link_libraries(dc_d2_1_dawn_first_render PRIVATE dc_gpu)
    add_test(NAME dc_d2_1_dawn_first_render COMMAND dc_d2_1_dawn_first_render)

    # ENC-485 (P2.2) — Dawn streaming buffer upload (coalesced dirty ranges).
    # Streams data into a Dawn buffer through the shared CpuBufferStore coalescing
    # path and a DeviceBufferResolver, asserts the same coalescing stats as the GL
    # d81_3_range_upload, and verifies correctness by reading the Dawn buffer back.
    # Links ONLY dc_gpu (no dc_gl): the CPU-side buffer manager is now in pure dc,
    # so the Dawn streaming path needs no GL header.
    add_executable(dc_d85_dawn_range_upload
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d85_dawn_range_upload.cpp"
    )
    target_link_libraries(dc_d85_dawn_range_upload PRIVATE dc_gpu)
    add_test(NAME dc_d85_dawn_range_upload COMMAND dc_d85_dawn_range_upload)

    # ENC-486 (P2.3) — line2d@1 + points@1 end-to-end on Dawn. Renders a line2d
    # scene and a points scene through the backend registry with DeviceKind::Dawn
    # into the headless DawnDevice offscreen target, reads back, and asserts the
    # 1px LineList / PointList coverage lands where expected with clear elsewhere.
    # ENC-501: CPU-side vertex bytes come from dc::CpuBufferStore (pure dc); links
    # only dc_gpu.
    add_executable(dc_d2_3_dawn_pipelines
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_3_dawn_pipelines.cpp"
    )
    target_link_libraries(dc_d2_3_dawn_pipelines PRIVATE dc_gpu)
    add_test(NAME dc_d2_3_dawn_pipelines COMMAND dc_d2_3_dawn_pipelines)

    # ENC-487 (P2.4) — triGradient@1 + triAA@1 on Dawn. Renders a per-vertex-color
    # gradient triangle and per-vertex-alpha triAA triangles through the backend
    # registry with DeviceKind::Dawn into the headless DawnDevice, reads back, and
    # asserts gradient corner-color trend + 3-way-blend centroid (vs the GL
    # d28_3_gradient baseline) and the AA per-vertex-alpha plumbing. Links only
    # dc_gpu: the CPU-side CpuBufferStore lives in pure dc (no GL header needed).
    add_executable(dc_d28_3_dawn_gradient
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_3_dawn_gradient.cpp"
    )
    target_link_libraries(dc_d28_3_dawn_gradient PRIVATE dc_gpu)
    add_test(NAME dc_d28_3_dawn_gradient COMMAND dc_d28_3_dawn_gradient)

    # ENC-488 (P2.5) — instancedRect@1 + rounded corners (D28.2) + indexed gather
    # (D26) on Dawn. First instanced pipeline: exercises the shared drawInstanced
    # foundation (per-instance step-mode buffer + instanced draw) plus the
    # rounded-corner SDF and the CPU-side D26 gather. Renders rects through the
    # backend registry with DeviceKind::Dawn into the headless DawnDevice, reads
    # back, and asserts filled interiors / rounded corners / selected subset (vs
    # the GL d28_2_rounded_rect + d26_2_indexed_gl baselines). Links only dc_gpu.
    add_executable(dc_d28_2_dawn_rounded_rect
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_2_dawn_rounded_rect.cpp"
    )
    target_link_libraries(dc_d28_2_dawn_rounded_rect PRIVATE dc_gpu)
    add_test(NAME dc_d28_2_dawn_rounded_rect COMMAND dc_d28_2_dawn_rounded_rect)

    # ENC-491 (P2.8) — texturedQuad@1 + TextureManager on Dawn. Exercises the new
    # DawnDevice texture/sampler support (createTexture RGBA8Unorm +
    # queue.WriteTexture + sampler + the texture/sampler bind-group entries) and
    # the DawnTexturedQuadBackend, which reuses the shared drawInstanced
    # foundation. Renders a textured quad sampling a small known RGBA texture
    # through the backend registry with DeviceKind::Dawn into the headless
    # DawnDevice, reads back, and asserts the sampled texels land at the expected
    # screen positions + color modulation (vs the GL d36_1_texture_quad /
    # d41_1_texquad_render baselines). Links only dc_gpu.
    add_executable(dc_d36_1_dawn_texquad
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d36_1_dawn_texquad.cpp"
    )
    target_link_libraries(dc_d36_1_dawn_texquad PRIVATE dc_gpu)
    add_test(NAME dc_d36_1_dawn_texquad COMMAND dc_d36_1_dawn_texquad)

    # ENC-489 (P2.6) — instancedCandle@1 on Dawn. Renders a row of OHLC candles
    # (mix of up/down, each with a wick) through the backend registry with
    # DeviceKind::Dawn into the headless DawnDevice, reads back, and asserts body
    # up/down color at the body center, wicks present above/below the body (thin
    # vertical lines at high/low), and clear gaps between candles (vs the GL
    # drawInstancedCandle / d14_6_volume_gl semantics). Reuses the shared
    # drawInstanced foundation. Links only dc_gpu.
    add_executable(dc_d14_6_dawn_candle
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_6_dawn_candle.cpp"
    )
    target_link_libraries(dc_d14_6_dawn_candle PRIVATE dc_gpu)
    add_test(NAME dc_d14_6_dawn_candle COMMAND dc_d14_6_dawn_candle)

    # ENC-490 (P2.7) — lineAA@1 (anti-aliased thick lines) + dash patterns
    # (D28.1) on Dawn. Quad-expands each segment into a thick AA quad in the
    # vertex shader and does AA coverage falloff + dash on/off in the fragment
    # shader (WebGPU has no native line width). Renders a solid thick AA line and
    # a dashed line through the backend registry with DeviceKind::Dawn into the
    # headless DawnDevice, reads back, and asserts line-center color, AA-edge
    # fade, and dash on/off segments (vs the GL d28_1_dashed_lines / d10_6_aa_lines
    # baselines). Links only dc_gpu.
    add_executable(dc_d28_1_dawn_lineaa
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_1_dawn_lineaa.cpp"
    )
    target_link_libraries(dc_d28_1_dawn_lineaa PRIVATE dc_gpu)
    add_test(NAME dc_d28_1_dawn_lineaa COMMAND dc_d28_1_dawn_lineaa)

    # ENC-493 (P3.1) — blend modes as immutable pipeline permutations (D29.1).
    # WebGPU bakes blend state into the immutable pipeline (unlike GL's per-draw
    # glBlendFunc), so the 4 per-DrawItem blend modes become pipeline VARIANTS
    # behind a permutation cache keyed on (base pipeline, blendMode). Draws a base
    # triSolid shape then an overlapping shape in each of Normal/Additive/Multiply/
    # Screen through the backend registry with DeviceKind::Dawn into the headless
    # DawnDevice (driving the mode via DawnDevice::setBlendMode, which the variant
    # selection in bindPipeline honors), reads back the overlap region, and asserts
    # the composited result matches each mode's math with known src/dst colors (vs
    # the GL d29_1_blend_modes baseline). Links only dc_gpu.
    add_executable(dc_d29_1_dawn_blend
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_1_dawn_blend.cpp"
    )
    target_link_libraries(dc_d29_1_dawn_blend PRIVATE dc_gpu)
    add_test(NAME dc_d29_1_dawn_blend COMMAND dc_d29_1_dawn_blend)

    # ENC-494 (P3.2) — stencil clipping masks as pipeline permutations (D29.2).
    # GL clips via a two-pass stencil (clip-source pass writes a mask with
    # compare Always / op Replace / color OFF, clipped pass draws where stencil
    # == 1 with compare Equal); WebGPU bakes the stencil compare/op + color-write
    # mask into the immutable pipeline, so ClipMode (None/WriteMask/UseMask)
    # becomes a second permutation axis on the ENC-493 variant cache — the key
    # grows to (blend, clip). The DawnDevice offscreen target gains a Stencil8
    # depth-stencil attachment (cleared per pass, matching GL's per-pane stencil
    # clear). This test draws a small clip-source triangle in WriteMask (stencil
    # only, no color) then a full-viewport content triangle in UseMask (drawn
    # only where stencil == 1) into the headless DawnDevice, reads back the
    # center (inside the clip -> content shows) and a corner (outside -> clipped
    # to the background), asserting content appears ONLY inside the clip region
    # (vs the GL d29_2_clipping baseline). Links only dc_gpu.
    add_executable(dc_d29_2_dawn_clipping
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_2_dawn_clipping.cpp"
    )
    target_link_libraries(dc_d29_2_dawn_clipping PRIVATE dc_gpu)
    add_test(NAME dc_d29_2_dawn_clipping COMMAND dc_d29_2_dawn_clipping)

    # ENC-495 (P3.3) — GPU color-ID picking (D29.3) on Dawn. Renders each
    # pickable DrawItem's id as a unique RGB color into a SEPARATE offscreen pick
    # target (DawnDevice multi-target support), reads back the probed pixel and
    # decodes the id (cross-checks the GL d29_3_gpu_picking / d41_2_texquad_pick
    # baselines). Asserts a hit decodes to the right id, a clear pixel decodes to
    # 0, distinct items decode to distinct ids, and a GeometryClicked event fires
    # on hit. Links only dc_gpu.
    add_executable(dc_d29_3_dawn_picking
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_3_dawn_picking.cpp"
    )
    target_link_libraries(dc_d29_3_dawn_picking PRIVATE dc_gpu)
    add_test(NAME dc_d29_3_dawn_picking COMMAND dc_d29_3_dawn_picking)

    # ENC-496 (P3.4) — post-process passes (D47 / D78.2) on Dawn. The LAST
    # Phase-3 ticket: renders a scene into an offscreen render target (the ENC-495
    # multi-target support, with the color texture now carrying TextureBinding
    # usage), then chains fullscreen effect passes — each samples the previous
    # pass's output texture (DawnPostProcessPass: a no-vertex-buffer fullscreen
    # triangle from @builtin(vertex_index) + the ENC-491 sampledTexture path) and
    # applies a builtin effect. Asserts the vignette darkens the corners (center >
    # corner) and the blur turns a sharp edge into a gradient (intermediate edge
    # pixel), cross-checking the GL d47_1_post_process / d47_2_blur_pass /
    # d78_2_builtin_effects baselines. Links only dc_gpu.
    add_executable(dc_d47_1_dawn_postprocess
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d47_1_dawn_postprocess.cpp"
    )
    target_link_libraries(dc_d47_1_dawn_postprocess PRIVATE dc_gpu)
    add_test(NAME dc_d47_1_dawn_postprocess COMMAND dc_d47_1_dawn_postprocess)

    # ENC-492 (P2.9) — textSDF@1 + glyph atlas upload on Dawn. The LAST Phase-2
    # pipeline: completes all 10 pipelines on WebGPU. Uploads the GlyphAtlas R8
    # SDF bitmap to a Dawn R8 texture (ENC-491 createTexture(R8) +
    # queue.WriteTexture, re-uploaded on atlas-dirty, mirroring the GL
    # uploadAtlasIfDirty) and samples it in the SDF shader; the fragment
    # reconstructs the SDF coverage alpha (smoothstep around the 0.5 isoline) so
    # the text edges are crisp + anti-aliased, composited by the default Normal
    # blend. Renders "Hi" glyphs through the backend registry with
    # DeviceKind::Dawn into the headless DawnDevice, reads back, and asserts glyph
    # stroke pixels are the text color (SDF alpha ~1 inside), background/outside
    # pixels clear, and the SDF edge produces partial-coverage (anti-aliased)
    # pixels (vs the GL d3_3_text_sdf semantics). Needs the test font. Links only
    # dc_gpu (GlyphAtlas lives in dc, which dc_gpu links).
    if (EXISTS "${DC_TEST_FONT}")
      add_executable(dc_d3_3_dawn_text_sdf
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/d3_3_dawn_text_sdf.cpp"
      )
      target_link_libraries(dc_d3_3_dawn_text_sdf PRIVATE dc_gpu)
      target_compile_definitions(dc_d3_3_dawn_text_sdf PRIVATE
        FONT_PATH="${DC_TEST_FONT}"
      )
      add_test(NAME dc_d3_3_dawn_text_sdf COMMAND dc_d3_3_dawn_text_sdf)
    endif()

    # ENC-509 (P5.0a) — DawnSceneRenderer: full scene-walk dispatch on Dawn. The
    # reusable Dawn mirror of GL Renderer::render: a single DawnSceneRenderer owns
    # a DawnDevice + all 10 Dawn pipeline backends and walks a whole multi-pipeline
    # Scene (built via CommandProcessor JSON) into the headless offscreen target,
    # dispatching every DrawItem through the backend registry (clear, per-pane
    # scissor, per-item frustum-cull + blend + clip + transform). Renders a
    # triSolid triangle + an instancedRect + a line2d (+ optional textSDF "Hi" when
    # the test font is present) in ONE render() call, reads back, and asserts each
    # element lands at its expected location/color with clear gaps between them —
    # proving the full walk dispatches every pipeline class. The keystone for
    # ENC-510 (conformance) / ENC-500 (cutover). Links only dc_gpu (which links dc
    # for Scene/CommandProcessor/GlyphAtlas).
    add_executable(dc_d50_dawn_scene_renderer
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d50_dawn_scene_renderer.cpp"
    )
    target_link_libraries(dc_d50_dawn_scene_renderer PRIVATE dc_gpu)
    if (EXISTS "${DC_TEST_FONT}")
      target_compile_definitions(dc_d50_dawn_scene_renderer PRIVATE
        FONT_PATH="${DC_TEST_FONT}"
      )
    endif()
    add_test(NAME dc_d50_dawn_scene_renderer COMMAND dc_d50_dawn_scene_renderer)

    # ENC-501 (P5 cutover) — Dawn-only golden conformance coverage. Formerly the
    # ENC-510/511/512/500 GL<->Dawn parity tests (rendered each scene through BOTH
    # the GL backend (Renderer + OsMesaContext) and the Dawn backend and compared
    # the readbacks). Dawn is now the proven default renderer and dc_gl is being
    # deleted, so these render ONLY via Dawn (parity_golden.hpp) and assert the
    # readback against captured GOLDEN probe pixels (the same Dawn pixels that
    # matched GL within tolerance while dc_gl existed). They link ONLY dc_gpu — no
    # dc_gl / OSMesa — and skip gracefully if Dawn is unavailable.
    add_executable(dc_parity_conformance
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_conformance.cpp"
    )
    target_link_libraries(dc_parity_conformance PRIVATE dc_gpu)
    add_test(NAME dc_parity_conformance COMMAND dc_parity_conformance)

    add_executable(dc_parity_text
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_text.cpp"
    )
    target_link_libraries(dc_parity_text PRIVATE dc_gpu)
    if (EXISTS "${DC_TEST_FONT}")
      target_compile_definitions(dc_parity_text PRIVATE
        FONT_PATH="${DC_TEST_FONT}"
      )
    endif()
    add_test(NAME dc_parity_text COMMAND dc_parity_text)

    # ENC-501 (was ENC-511) — Dawn-only golden multi-pane: per-pane clear colors +
    # D78 pane borders/separators (DawnSceneRenderer), probe-pixel goldens.
    add_executable(dc_parity_multipane
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_multipane.cpp"
    )
    target_link_libraries(dc_parity_multipane PRIVATE dc_gpu)
    add_test(NAME dc_parity_multipane COMMAND dc_parity_multipane)

    # ENC-501 (was ENC-512) — Dawn-only golden extended: texturedQuad@1 (texture
    # via TextureSource), GPU picking (decoded id-buffer self-validation),
    # indexed-gather (D26 index/order buffer filtering instanced draws), and a
    # recipe-driven full chart (CandleRecipe + LineRecipe). Recipe sources are in
    # dc (linked by dc_gpu).
    add_executable(dc_parity_extended
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity_extended.cpp"
    )
    target_link_libraries(dc_parity_extended PRIVATE dc_gpu)
    add_test(NAME dc_parity_extended COMMAND dc_parity_extended)

    # ENC-501 (was ENC-500) — Dawn-only JsonHost golden conformance + streaming
    # sweep. Drives the JsonHost embedding pipeline (parseSceneDocument ->
    # SceneReconciler -> IngestProcessor -> store -> render -> readback) on the Dawn
    # backend and asserts the readback against captured golden probe pixels, then
    # times N writeRange+uploadDirty cycles on the Dawn streaming hot path.
    add_executable(dc_d79_dawn_json_host
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d79_dawn_json_host.cpp"
    )
    target_link_libraries(dc_d79_dawn_json_host PRIVATE dc_gpu)
    add_test(NAME dc_d79_dawn_json_host COMMAND dc_d79_dawn_json_host)
  endif()
endif()

# =======================
# dc_wasm (ENC-502) — Emscripten/WASM binding skeleton for the `dc` logic core
# =======================
# Compiles the pure backend-agnostic `dc` core (scene graph, command processor,
# ingest, recipes, document — NO graphics) to WebAssembly and exposes a minimal
# JS/TS surface via Embind. This proves the dc logic runs in WASM and round-trips
# a JSON command across the JS boundary. NO WebGPU here — the Dawn renderer
# (dc_gpu on emdawnwebgpu) lands in ENC-503 on top of the same DcCore.
#
# Gated on Emscripten so it never affects the native build. The native default
# build (no Emscripten toolchain) skips this entirely.
if (EMSCRIPTEN)
  # Single-threaded build (simplest that compiles): we do NOT pass -pthread, so
  # the WASM module is built without SharedArrayBuffer/worker threads. The only
  # std::thread user in pure `dc` is FakeDataSource (a demo data source the
  # binding skeleton never touches); the scene/command/ingest/document logic the
  # binding exposes is single-threaded. Under Emscripten, find_package(Threads)
  # provides a Threads::Threads target that is a no-op without -pthread, so `dc`
  # linking it does not force pthreads on.
  add_executable(dc_wasm "${CMAKE_CURRENT_SOURCE_DIR}/wasm/dc_wasm.cpp")
  target_link_libraries(dc_wasm PRIVATE dc)

  # Emscripten link flags:
  #   --bind            : enable Embind (the binding uses EMSCRIPTEN_BINDINGS)
  #   MODULARIZE=1      : emit a factory function instead of a global Module
  #   EXPORT_ES6=1      : emit an ES6 module (import the default export factory)
  #   EXPORT_NAME       : name of the exported module factory
  #   ENVIRONMENT       : node + web (this ticket validates under node)
  #   ALLOW_MEMORY_GROWTH : let the heap grow (the core allocates scene state)
  #   -sEXPORTED_RUNTIME_METHODS: not needed — Embind wires the class surface.
  # No -pthread (single-threaded, see note above). Exceptions: dc uses C++
  # exceptions internally (e.g. RapidJSON / std), so enable them on the link.
  target_compile_options(dc_wasm PRIVATE -fexceptions)
  target_link_options(dc_wasm PRIVATE
    "--bind"
    "-fexceptions"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createDcCore"
    "-sENVIRONMENT=node,web"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sDISABLE_EXCEPTION_CATCHING=0"
  )
  # Emit dc_wasm.js (+ dc_wasm.wasm). With EXPORT_ES6 the .js is an ES module.
  set_target_properties(dc_wasm PROPERTIES
    OUTPUT_NAME "dc_wasm"
    SUFFIX ".js"
  )
  message(STATUS "dc_wasm (Emscripten WASM binding skeleton) enabled")
endif()

# =======================
# dc_webgpu (ENC-503) — Emscripten/emdawnwebgpu build of the dc_gpu triSolid
# render path: the FIRST browser WebGPU render in the migration (P6.2).
# =======================
# Compiles the SAME dc_gpu rendering code that runs on native Dawn — DawnDevice's
# webgpu_cpp render path (pipelines/buffers/draw/copy-to-buffer readback + WGSL)
# and DawnTriSolidBackend — but against Emscripten's emdawnwebgpu port instead of
# native dawn::webgpu_dawn. This proves the Dawn/webgpu_cpp rendering code is
# browser-portable. Only DEVICE ACQUISITION + the readback event-pump differ
# in-browser; both are #ifdef __EMSCRIPTEN__ branches inside DawnDevice (browser
# RequestAdapter/RequestDevice off navigator.gpu; emscripten_sleep pump).
#
# Gated on Emscripten like dc_wasm so it never affects the native build. It does
# NOT use DC_FETCH_DAWN/dc_gpu (those build native Dawn from source); instead it
# compiles the two needed gpu/ translation units directly and links emdawnwebgpu.
if (EMSCRIPTEN)
  # The dc_gpu render code we exercise: DawnDevice (device + render pass + draw +
  # readback) and the triSolid backend. We compile these TUs into the executable
  # directly (rather than building the whole dc_gpu library, which globs every
  # backend) — triSolid is the first-render slice this ticket targets.
  add_executable(dc_webgpu
    "${CMAKE_CURRENT_SOURCE_DIR}/wasm/dc_webgpu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/DawnDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/DawnTriSolidBackend.cpp"
  )
  target_link_libraries(dc_webgpu PRIVATE dc)
  target_include_directories(dc_webgpu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  # webgpu_cpp.h needs C++20 (concepts/requires in its callback helpers), same as
  # the native dc_gpu library.
  target_compile_features(dc_webgpu PRIVATE cxx_std_20)

  # emdawnwebgpu provides <webgpu/webgpu_cpp.h> backed by navigator.gpu. The port
  # flag must be on BOTH compile and link (it injects the JS library + headers).
  target_compile_options(dc_webgpu PRIVATE
    "--use-port=emdawnwebgpu"
    -fexceptions
  )
  # Emscripten link flags:
  #   --use-port=emdawnwebgpu : link the emdawnwebgpu runtime (navigator.gpu).
  #   -sASYNCIFY              : let DawnDevice::init() + readback yield to the JS
  #                             event loop (emscripten_sleep) so the synchronous
  #                             native API shape works in-browser. REQUIRED.
  #   MODULARIZE/EXPORT_ES6   : ES6 module factory (default export) named
  #                             createDcWebGpu — mirrors dc_wasm (ENC-502).
  #   EXPORTED_RUNTIME_METHODS: ccall (the page awaits dc_webgpu_render async) +
  #                             HEAPU8 (the page reads the framebuffer bytes).
  #   EXPORTED_FUNCTIONS      : keep the C render entry point + malloc/free.
  #   ENVIRONMENT=node,web    : load under node (module-instantiation check) + web
  #                             (the real browser render).
  target_link_options(dc_webgpu PRIVATE
    "--use-port=emdawnwebgpu"
    "--bind"
    "-fexceptions"
    "-sASYNCIFY=1"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createDcWebGpu"
    "-sENVIRONMENT=node,web"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8"
    "-sEXPORTED_FUNCTIONS=_dc_webgpu_render,_malloc,_free"
  )
  set_target_properties(dc_webgpu PROPERTIES
    OUTPUT_NAME "dc_webgpu"
    SUFFIX ".js"
  )
  message(STATUS "dc_webgpu (Emscripten emdawnwebgpu triSolid render) enabled")
endif()

# =======================
# dc_webgpu_all (ENC-504) — Emscripten/emdawnwebgpu build of the FULL renderer:
# DawnSceneRenderer + ALL 10 Dawn pipeline backends. Browser pipeline parity (P6.3).
# =======================
# ENC-503 (dc_webgpu above) compiled only the triSolid slice (DawnDevice +
# DawnTriSolidBackend). This extends that to the WHOLE renderer: it compiles
# DawnSceneRenderer + EVERY gpu/ backend translation unit (triSolid / triGradient
# / triAA / line2d / lineAA / points / instancedRect / instancedCandle / textSDF /
# texturedQuad + pick + post-process) against emdawnwebgpu. The KEY RESULT is that
# all 10 pipeline backends compile + link for the browser — i.e. the entire Dawn
# renderer is browser-portable, not just the first-render triSolid path.
#
# The harness (dc_webgpu_all.cpp) drives a multi-pipeline Scene through
# DawnSceneRenderer::render() — the same scene-walk the native
# d50_dawn_scene_renderer test uses — and reads back each region.
#
# Like dc_webgpu it is gated on Emscripten (never affects the native build) and
# does NOT use DC_FETCH_DAWN/dc_gpu (those build native Dawn from source);
# instead it globs the gpu/ TUs directly and links emdawnwebgpu. placeholder.cpp
# is excluded (it pulls gpu/Gpu.hpp, a non-renderer stub).
if (EMSCRIPTEN)
  file(GLOB DC_WEBGPU_ALL_GPU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/*.cpp")
  list(FILTER DC_WEBGPU_ALL_GPU_SOURCES EXCLUDE REGEX ".*/placeholder\\.cpp$")
  # ENC-497: DawnWindowContext (GLFW/X11 on-screen surface) is native-windowed only.
  list(FILTER DC_WEBGPU_ALL_GPU_SOURCES EXCLUDE REGEX ".*/DawnWindowContext\\.cpp$")

  add_executable(dc_webgpu_all
    "${CMAKE_CURRENT_SOURCE_DIR}/wasm/dc_webgpu_all.cpp"
    ${DC_WEBGPU_ALL_GPU_SOURCES}
  )
  target_link_libraries(dc_webgpu_all PRIVATE dc)
  target_include_directories(dc_webgpu_all PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  # webgpu_cpp.h needs C++20 (same as the native dc_gpu library).
  target_compile_features(dc_webgpu_all PRIVATE cxx_std_20)

  # emdawnwebgpu provides <webgpu/webgpu_cpp.h> backed by navigator.gpu. The port
  # flag must be on BOTH compile and link.
  target_compile_options(dc_webgpu_all PRIVATE
    "--use-port=emdawnwebgpu"
    -fexceptions
  )
  # Same link flags as dc_webgpu (ASYNCIFY device acquisition + readback pump,
  # ES6 module factory, ccall/HEAPU8 runtime methods), with the new C entry
  # point dc_webgpu_render_all and module factory createDcWebGpuAll.
  target_link_options(dc_webgpu_all PRIVATE
    "--use-port=emdawnwebgpu"
    "--bind"
    "-fexceptions"
    "-sASYNCIFY=1"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createDcWebGpuAll"
    "-sENVIRONMENT=node,web"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8"
    "-sEXPORTED_FUNCTIONS=_dc_webgpu_render_all,_malloc,_free"
  )
  set_target_properties(dc_webgpu_all PROPERTIES
    OUTPUT_NAME "dc_webgpu_all"
    SUFFIX ".js"
  )
  message(STATUS "dc_webgpu_all (Emscripten emdawnwebgpu FULL renderer — all 10 pipelines) enabled")
endif()

# =======================
# dc_engine_host (ENC-506) — Emscripten/emdawnwebgpu build of the EngineHost-
# shaped WASM module: DcCore (applyControl + applyDataBatch) + the FULL Dawn
# renderer (DawnSceneRenderer + all backends) + pick, behind ONE Embind class
# (DcEngineHost) the @repo/dc-wasm TS EngineHost wrapper drives. (P6.5)
# =======================
# This is the WASM artifact @repo/dc-wasm ships. Unlike dc_webgpu_all (a fixed
# demo scene), dc_engine_host is STATEFUL and DATA-DRIVEN: JS feeds it JSON
# commands (applyControl) + binary record batches (applyDataBatch), then asks it
# to render() the live scene / pick(). It compiles the same gpu/ TUs + emdawnwebgpu
# as dc_webgpu_all (the whole renderer is browser-portable), plus the new
# wasm/dc_engine_host.cpp binding. The browser canvas is bound JS-side: the TS
# EngineHost blits the read-back framebuffer onto the caller's <canvas>.
#
# Gated on Emscripten like dc_wasm / dc_webgpu_all — never affects the native build.
if (EMSCRIPTEN)
  file(GLOB DC_ENGINE_HOST_GPU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/*.cpp")
  list(FILTER DC_ENGINE_HOST_GPU_SOURCES EXCLUDE REGEX ".*/placeholder\\.cpp$")
  # ENC-497: DawnWindowContext (GLFW/X11 on-screen surface) is native-windowed only.
  list(FILTER DC_ENGINE_HOST_GPU_SOURCES EXCLUDE REGEX ".*/DawnWindowContext\\.cpp$")

  add_executable(dc_engine_host
    "${CMAKE_CURRENT_SOURCE_DIR}/wasm/dc_engine_host.cpp"
    ${DC_ENGINE_HOST_GPU_SOURCES}
  )
  target_link_libraries(dc_engine_host PRIVATE dc)
  target_include_directories(dc_engine_host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  # webgpu_cpp.h needs C++20 (same as the native dc_gpu library).
  target_compile_features(dc_engine_host PRIVATE cxx_std_20)

  target_compile_options(dc_engine_host PRIVATE
    "--use-port=emdawnwebgpu"
    -fexceptions
  )
  # Same link shape as dc_webgpu_all (ASYNCIFY device acquisition + readback pump,
  # ES6 module factory, ccall/HEAPU8 runtime methods). EXPORT_NAME createDcEngineHost;
  # no extra C entry point (the surface is the Embind DcEngineHost class).
  target_link_options(dc_engine_host PRIVATE
    "--use-port=emdawnwebgpu"
    "--bind"
    "-fexceptions"
    "-sASYNCIFY=1"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createDcEngineHost"
    "-sENVIRONMENT=node,web"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8"
    "-sEXPORTED_FUNCTIONS=_malloc,_free"
  )
  set_target_properties(dc_engine_host PROPERTIES
    OUTPUT_NAME "dc_engine_host"
    SUFFIX ".js"
  )
  message(STATUS "dc_engine_host (Emscripten emdawnwebgpu EngineHost module — ENC-506) enabled")
endif()
