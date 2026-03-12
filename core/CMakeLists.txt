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
# Core library (no GL deps)
# =======================
file(GLOB_RECURSE DC_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# Exclude gl/ sources — those belong to dc_gl.
list(FILTER DC_SOURCES EXCLUDE REGEX ".*/gl/.*")
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
# GLAD (GL loader)
# =======================
set(GLAD_DIR "${THIRD_PARTY_ROOT}/glad")
if (EXISTS "${GLAD_DIR}/src/gl.c")
  add_library(glad STATIC "${GLAD_DIR}/src/gl.c")
  target_include_directories(glad PUBLIC "${GLAD_DIR}/include")
  set(DC_HAS_GLAD TRUE)
else()
  set(DC_HAS_GLAD FALSE)
  message(STATUS "GLAD not found at ${GLAD_DIR} — dc_gl will not be built")
endif()

# =======================
# OSMesa (headless GL)
# =======================
find_path(OSMESA_INCLUDE_DIR GL/osmesa.h)
find_library(OSMESA_LIBRARY NAMES OSMesa)

if (OSMESA_INCLUDE_DIR AND OSMESA_LIBRARY)
  set(DC_HAS_OSMESA TRUE)
  message(STATUS "OSMesa found: ${OSMESA_LIBRARY}")
else()
  set(DC_HAS_OSMESA FALSE)
  message(STATUS "OSMesa not found")
endif()

# =======================
# GLFW (windowed GL)
# =======================
find_package(glfw3 3.3 QUIET)
if (glfw3_FOUND)
  set(DC_HAS_GLFW TRUE)
  message(STATUS "GLFW found")
else()
  set(DC_HAS_GLFW FALSE)
  message(STATUS "GLFW not found")
endif()

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
# dc_gl library (GL rendering backend)
# =======================
if (DC_HAS_GLAD AND (DC_HAS_OSMESA OR DC_HAS_GLFW))
  # Collect all gl sources
  file(GLOB DC_GL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/gl/*.cpp")

  # Conditionally exclude context files based on availability
  if (NOT DC_HAS_OSMESA)
    list(FILTER DC_GL_SOURCES EXCLUDE REGEX ".*OsMesaContext\\.cpp$")
  endif()
  if (NOT DC_HAS_GLFW)
    list(FILTER DC_GL_SOURCES EXCLUDE REGEX ".*GlfwContext\\.cpp$")
  endif()

  add_library(dc_gl STATIC ${DC_GL_SOURCES})
  target_include_directories(dc_gl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(dc_gl PUBLIC dc glad)

  if (DC_HAS_OSMESA)
    target_include_directories(dc_gl PUBLIC "${OSMESA_INCLUDE_DIR}")
    target_link_libraries(dc_gl PUBLIC ${OSMESA_LIBRARY})
    target_compile_definitions(dc_gl PUBLIC DC_HAS_OSMESA=1)
  endif()

  if (DC_HAS_GLFW)
    target_link_libraries(dc_gl PUBLIC glfw)
    target_compile_definitions(dc_gl PUBLIC DC_HAS_GLFW=1)
  endif()

  if (NOT MSVC)
    target_compile_options(dc_gl PRIVATE -Wall -Wextra -Wpedantic)
    if (DC_WARNINGS_AS_ERRORS)
      target_compile_options(dc_gl PRIVATE -Werror)
    endif()
  endif()

  # D2.7 demo
  add_executable(dc_d2_7_demo "${CMAKE_CURRENT_SOURCE_DIR}/demos/d2_7_candle_demo.cpp")
  target_link_libraries(dc_d2_7_demo PRIVATE dc_gl)

  # D3.5 recipe demo
  add_executable(dc_d3_5_recipe_demo "${CMAKE_CURRENT_SOURCE_DIR}/demos/d3_5_recipe_demo.cpp")
  target_link_libraries(dc_d3_5_recipe_demo PRIVATE dc_gl)
  if (EXISTS "${THIRD_PARTY_ROOT}/test_font.ttf")
    target_compile_definitions(dc_d3_5_recipe_demo PRIVATE
      FONT_PATH="${THIRD_PARTY_ROOT}/test_font.ttf"
    )
  endif()

  # D4.6 full demo
  add_executable(dc_d4_6_full_demo "${CMAKE_CURRENT_SOURCE_DIR}/demos/d4_6_full_demo.cpp")
  target_link_libraries(dc_d4_6_full_demo PRIVATE dc_gl)
  if (EXISTS "${THIRD_PARTY_ROOT}/test_font.ttf")
    target_compile_definitions(dc_d4_6_full_demo PRIVATE
      FONT_PATH="${THIRD_PARTY_ROOT}/test_font.ttf"
    )
  endif()

  # D5.7 interactive demo
  add_executable(dc_d5_7_interactive_demo "${CMAKE_CURRENT_SOURCE_DIR}/demos/d5_7_interactive_demo.cpp")
  target_link_libraries(dc_d5_7_interactive_demo PRIVATE dc_gl)
  if (EXISTS "${THIRD_PARTY_ROOT}/test_font.ttf")
    target_compile_definitions(dc_d5_7_interactive_demo PRIVATE
      FONT_PATH="${THIRD_PARTY_ROOT}/test_font.ttf"
    )
  endif()

  # D6.3 live streaming demo
  add_executable(dc_d6_3_live_demo "${CMAKE_CURRENT_SOURCE_DIR}/demos/d6_3_live_demo.cpp")
  target_link_libraries(dc_d6_3_live_demo PRIVATE dc_gl)
  if (DC_HAS_WEBSOCKET AND TARGET dc_ws)
    target_link_libraries(dc_d6_3_live_demo PRIVATE dc_ws)
  endif()
  if (EXISTS "${THIRD_PARTY_ROOT}/test_font.ttf")
    target_compile_definitions(dc_d6_3_live_demo PRIVATE
      FONT_PATH="${THIRD_PARTY_ROOT}/test_font.ttf"
    )
  endif()

  # Live server (OSMesa rendering server for web viewer)
  add_executable(dc_live_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/live_server.cpp")
  target_link_libraries(dc_live_server PRIVATE dc_gl)

  # Showcase server (multi-chart demo: line + pie + table)
  add_executable(dc_showcase_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/showcase_server.cpp")
  target_link_libraries(dc_showcase_server PRIVATE dc_gl)

  # Gallery demo (renders 6 showcase PNGs)
  add_executable(dc_gallery "${CMAKE_CURRENT_SOURCE_DIR}/demos/gallery.cpp")
  target_link_libraries(dc_gallery PRIVATE dc_gl)

  # D76 Feature Showcase (all 13 new subsystems)
  add_executable(dc_d76_showcase "${CMAKE_CURRENT_SOURCE_DIR}/demos/d76_showcase.cpp")
  target_link_libraries(dc_d76_showcase PRIVATE dc_gl)

  # D77 Declarative JSON Scene Document demo
  add_executable(dc_d77_declarative "${CMAKE_CURRENT_SOURCE_DIR}/demos/d77_declarative.cpp")
  target_link_libraries(dc_d77_declarative PRIVATE dc_gl)

  # D78 Modern Chart Styling Showcase
  add_executable(dc_d78_showcase "${CMAKE_CURRENT_SOURCE_DIR}/demos/d78_showcase.cpp")
  target_link_libraries(dc_d78_showcase PRIVATE dc_gl)

  # Dashboard server (5-pane market command center)
  add_executable(dc_dashboard_server "${CMAKE_CURRENT_SOURCE_DIR}/demos/dashboard_server.cpp")
  target_link_libraries(dc_dashboard_server PRIVATE dc_gl)

  # D79 Generic JSON host (reads .json chart, serves via TEXT/FRME protocol)
  add_executable(dc_json_host "${CMAKE_CURRENT_SOURCE_DIR}/src/host/JsonHost.cpp")
  target_link_libraries(dc_json_host PRIVATE dc_gl)
else()
  message(STATUS "Skipping dc_gl (requires GLAD + at least one of OSMesa/GLFW)")
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

  # GL-dependent tests (only if dc_gl with OSMesa is available)
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    add_executable(dc_d2_1_first_render
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_1_first_render.cpp"
    )
    target_link_libraries(dc_d2_1_first_render PRIVATE dc_gl)
    add_test(NAME dc_d2_1_first_render COMMAND dc_d2_1_first_render)

    add_executable(dc_d2_2_transforms
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_2_transforms.cpp"
    )
    target_link_libraries(dc_d2_2_transforms PRIVATE dc_gl)
    add_test(NAME dc_d2_2_transforms COMMAND dc_d2_2_transforms)

    add_executable(dc_d2_3_pipelines
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_3_pipelines.cpp"
    )
    target_link_libraries(dc_d2_3_pipelines PRIVATE dc_gl)
    add_test(NAME dc_d2_3_pipelines COMMAND dc_d2_3_pipelines)

    add_executable(dc_d2_4_ingest
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d2_4_ingest.cpp"
    )
    target_link_libraries(dc_d2_4_ingest PRIVATE dc_gl)
    add_test(NAME dc_d2_4_ingest COMMAND dc_d2_4_ingest)

    if (EXISTS "${DC_TEST_FONT}")
      add_executable(dc_d3_3_text_sdf
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/d3_3_text_sdf.cpp"
      )
      target_link_libraries(dc_d3_3_text_sdf PRIVATE dc_gl)
      target_compile_definitions(dc_d3_3_text_sdf PRIVATE
        FONT_PATH="${DC_TEST_FONT}"
      )
      add_test(NAME dc_d3_3_text_sdf COMMAND dc_d3_3_text_sdf)
    endif()

    # D4.1 Area (GL test)
    add_executable(dc_d4_1_area
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_1_area.cpp"
    )
    target_link_libraries(dc_d4_1_area PRIVATE dc_gl)
    add_test(NAME dc_d4_1_area COMMAND dc_d4_1_area)

    # D4.6 Demo integration test (GL)
    add_executable(dc_d4_6_demo
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d4_6_demo.cpp"
    )
    target_link_libraries(dc_d4_6_demo PRIVATE dc_gl)
    add_test(NAME dc_d4_6_demo COMMAND dc_d4_6_demo)

    # D5.7 Interactive integration test (GL)
    add_executable(dc_d5_7_interactive
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d5_7_interactive.cpp"
    )
    target_link_libraries(dc_d5_7_interactive PRIVATE dc_gl)
    if (EXISTS "${DC_TEST_FONT}")
      target_compile_definitions(dc_d5_7_interactive PRIVATE
        FONT_PATH="${DC_TEST_FONT}"
      )
    endif()
    add_test(NAME dc_d5_7_interactive COMMAND dc_d5_7_interactive)

    # D6.3 Live streaming integration test (GL)
    add_executable(dc_d6_3_live
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d6_3_live.cpp"
    )
    target_link_libraries(dc_d6_3_live PRIVATE dc_gl)
    add_test(NAME dc_d6_3_live COMMAND dc_d6_3_live)

    # D7.4 ChartSession live integration test (GL)
    add_executable(dc_d7_4_session_live
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d7_4_session_live.cpp"
    )
    target_link_libraries(dc_d7_4_session_live PRIVATE dc_gl)
    add_test(NAME dc_d7_4_session_live COMMAND dc_d7_4_session_live)

    # D8.6 Aggregation GL integration test
    add_executable(dc_d8_6_agg_live
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d8_6_agg_live.cpp"
    )
    target_link_libraries(dc_d8_6_agg_live PRIVATE dc_gl)
    add_test(NAME dc_d8_6_agg_live COMMAND dc_d8_6_agg_live)

    # D9.2 GL scissor per pane
    add_executable(dc_d9_2_scissor
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_2_scissor.cpp"
    )
    target_link_libraries(dc_d9_2_scissor PRIVATE dc_gl)
    add_test(NAME dc_d9_2_scissor COMMAND dc_d9_2_scissor)

    # D9.6 Multi-pane GL integration test
    add_executable(dc_d9_6_multi_pane
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d9_6_multi_pane.cpp"
    )
    target_link_libraries(dc_d9_6_multi_pane PRIVATE dc_gl)
    add_test(NAME dc_d9_6_multi_pane COMMAND dc_d9_6_multi_pane)

    # D10.1 Style system (GL)
    add_executable(dc_d10_1_style
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_1_style.cpp"
    )
    target_link_libraries(dc_d10_1_style PRIVATE dc_gl)
    add_test(NAME dc_d10_1_style COMMAND dc_d10_1_style)

    # D10.4 Per-pane clear color (GL)
    add_executable(dc_d10_4_pane_clear
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_4_pane_clear.cpp"
    )
    target_link_libraries(dc_d10_4_pane_clear PRIVATE dc_gl)
    add_test(NAME dc_d10_4_pane_clear COMMAND dc_d10_4_pane_clear)

    # D10.5 Frustum culling (GL)
    add_executable(dc_d10_5_frustum_cull
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_5_frustum_cull.cpp"
    )
    target_link_libraries(dc_d10_5_frustum_cull PRIVATE dc_gl)
    add_test(NAME dc_d10_5_frustum_cull COMMAND dc_d10_5_frustum_cull)

    # D10.6 Anti-aliased lines (GL)
    add_executable(dc_d10_6_aa_lines
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d10_6_aa_lines.cpp"
    )
    target_link_libraries(dc_d10_6_aa_lines PRIVATE dc_gl)
    add_test(NAME dc_d10_6_aa_lines COMMAND dc_d10_6_aa_lines)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA AND EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d11_6_interaction
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d11_6_interaction.cpp"
    )
    target_link_libraries(dc_d11_6_interaction PRIVATE dc_gl)
    target_compile_definitions(dc_d11_6_interaction PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d11_6_interaction COMMAND dc_d11_6_interaction)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA AND EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d12_6_axis_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d12_6_axis_gl.cpp"
    )
    target_link_libraries(dc_d12_6_axis_gl PRIVATE dc_gl)
    target_compile_definitions(dc_d12_6_axis_gl PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d12_6_axis_gl COMMAND dc_d12_6_axis_gl)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA AND EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d13_5_time_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d13_5_time_gl.cpp"
    )
    target_link_libraries(dc_d13_5_time_gl PRIVATE dc_gl)
    target_compile_definitions(dc_d13_5_time_gl PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d13_5_time_gl COMMAND dc_d13_5_time_gl)
  endif()

  # D14.1 VolumeRecipe (pure C++)
  add_executable(dc_d14_1_volume
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_1_volume.cpp"
  )
  target_link_libraries(dc_d14_1_volume PRIVATE dc)
  add_test(NAME dc_d14_1_volume COMMAND dc_d14_1_volume)

  # D14.2 DrawItem visibility (GL)
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    add_executable(dc_d14_2_visibility
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_2_visibility.cpp"
    )
    target_link_libraries(dc_d14_2_visibility PRIVATE dc_gl)
    add_test(NAME dc_d14_2_visibility COMMAND dc_d14_2_visibility)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA AND EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d14_6_volume_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d14_6_volume_gl.cpp"
    )
    target_link_libraries(dc_d14_6_volume_gl PRIVATE dc_gl)
    target_compile_definitions(dc_d14_6_volume_gl PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d14_6_volume_gl COMMAND dc_d14_6_volume_gl)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    # D17.5 Indicator GL integration
    add_executable(dc_d17_5_indicator_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d17_5_indicator_gl.cpp"
    )
    target_link_libraries(dc_d17_5_indicator_gl PRIVATE dc_gl)
    add_test(NAME dc_d17_5_indicator_gl COMMAND dc_d17_5_indicator_gl)

    # D18.1 Export PPM (GL)
    add_executable(dc_d18_1_export
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d18_1_export.cpp"
    )
    target_link_libraries(dc_d18_1_export PRIVATE dc_gl)
    add_test(NAME dc_d18_1_export COMMAND dc_d18_1_export)

    # D18.2 Export PNG (GL)
    add_executable(dc_d18_2_png_export
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d18_2_png_export.cpp"
    )
    target_link_libraries(dc_d18_2_png_export PRIVATE dc_gl)
    add_test(NAME dc_d18_2_png_export COMMAND dc_d18_2_png_export)

    # D25.1 Showcase render validation (GL)
    add_executable(dc_d25_1_showcase
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d25_1_showcase.cpp"
    )
    target_link_libraries(dc_d25_1_showcase PRIVATE dc_gl)
    add_test(NAME dc_d25_1_showcase COMMAND dc_d25_1_showcase)

    # D26.2 Indexed GL rendering
    add_executable(dc_d26_2_indexed_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d26_2_indexed_gl.cpp"
    )
    target_link_libraries(dc_d26_2_indexed_gl PRIVATE dc_gl)
    add_test(NAME dc_d26_2_indexed_gl COMMAND dc_d26_2_indexed_gl)

    # D28.1 Dashed lines (GL)
    add_executable(dc_d28_1_dashed_lines
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_1_dashed_lines.cpp"
    )
    target_link_libraries(dc_d28_1_dashed_lines PRIVATE dc_gl)
    add_test(NAME dc_d28_1_dashed_lines COMMAND dc_d28_1_dashed_lines)

    # D28.2 Rounded rectangles (GL)
    add_executable(dc_d28_2_rounded_rect
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_2_rounded_rect.cpp"
    )
    target_link_libraries(dc_d28_2_rounded_rect PRIVATE dc_gl)
    add_test(NAME dc_d28_2_rounded_rect COMMAND dc_d28_2_rounded_rect)

    # D28.3 Gradient fills (GL)
    add_executable(dc_d28_3_gradient
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d28_3_gradient.cpp"
    )
    target_link_libraries(dc_d28_3_gradient PRIVATE dc_gl)
    add_test(NAME dc_d28_3_gradient COMMAND dc_d28_3_gradient)

    # D29.1 Blend modes (GL)
    add_executable(dc_d29_1_blend_modes
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_1_blend_modes.cpp"
    )
    target_link_libraries(dc_d29_1_blend_modes PRIVATE dc_gl)
    add_test(NAME dc_d29_1_blend_modes COMMAND dc_d29_1_blend_modes)

    # D29.2 Clipping masks (GL)
    add_executable(dc_d29_2_clipping
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_2_clipping.cpp"
    )
    target_link_libraries(dc_d29_2_clipping PRIVATE dc_gl)
    add_test(NAME dc_d29_2_clipping COMMAND dc_d29_2_clipping)

    # D29.3 GPU picking (GL)
    add_executable(dc_d29_3_gpu_picking
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d29_3_gpu_picking.cpp"
    )
    target_link_libraries(dc_d29_3_gpu_picking PRIVATE dc_gl)
    add_test(NAME dc_d29_3_gpu_picking COMMAND dc_d29_3_gpu_picking)

    # D33.2 Box selection GL
    add_executable(dc_d33_2_box_select_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d33_2_box_select_gl.cpp"
    )
    target_link_libraries(dc_d33_2_box_select_gl PRIVATE dc_gl)
    add_test(NAME dc_d33_2_box_select_gl COMMAND dc_d33_2_box_select_gl)

    # D36.1 Texture quad (GL)
    add_executable(dc_d36_1_texture_quad
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d36_1_texture_quad.cpp"
    )
    target_link_libraries(dc_d36_1_texture_quad PRIVATE dc_gl)
    add_test(NAME dc_d36_1_texture_quad COMMAND dc_d36_1_texture_quad)

    # D36.2 Texture manager (GL)
    add_executable(dc_d36_2_texture_manager
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d36_2_texture_manager.cpp"
    )
    target_link_libraries(dc_d36_2_texture_manager PRIVATE dc_gl)
    add_test(NAME dc_d36_2_texture_manager COMMAND dc_d36_2_texture_manager)

    # D37.2 Anchor GL
    add_executable(dc_d37_2_anchor_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d37_2_anchor_gl.cpp"
    )
    target_link_libraries(dc_d37_2_anchor_gl PRIVATE dc_gl)
    add_test(NAME dc_d37_2_anchor_gl COMMAND dc_d37_2_anchor_gl)

    # D42.2 Renderer event emission (GL)
    add_executable(dc_d42_2_renderer_events
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d42_2_renderer_events.cpp"
    )
    target_link_libraries(dc_d42_2_renderer_events PRIVATE dc_gl)
    add_test(NAME dc_d42_2_renderer_events COMMAND dc_d42_2_renderer_events)

    # D41.1 Textured quad render (GL)
    add_executable(dc_d41_1_texquad_render
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d41_1_texquad_render.cpp"
    )
    target_link_libraries(dc_d41_1_texquad_render PRIVATE dc_gl)
    add_test(NAME dc_d41_1_texquad_render COMMAND dc_d41_1_texquad_render)

    # D41.2 Textured quad pick (GL)
    add_executable(dc_d41_2_texquad_pick
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d41_2_texquad_pick.cpp"
    )
    target_link_libraries(dc_d41_2_texquad_pick PRIVATE dc_gl)
    add_test(NAME dc_d41_2_texquad_pick COMMAND dc_d41_2_texquad_pick)
  endif()

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

  # D56.1 BatchBuilder (needs dc_gl for BatchBuilder)
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    add_executable(dc_d56_1_batching
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d56_1_batching.cpp"
    )
    target_link_libraries(dc_d56_1_batching PRIVATE dc_gl)
    add_test(NAME dc_d56_1_batching COMMAND dc_d56_1_batching)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    add_executable(dc_d54_2_debug_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d54_2_debug_gl.cpp"
    )
    target_link_libraries(dc_d54_2_debug_gl PRIVATE dc_gl)
    add_test(NAME dc_d54_2_debug_gl COMMAND dc_d54_2_debug_gl)

    # D47.1 PostProcess brightness (GL)
    add_executable(dc_d47_1_post_process
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d47_1_post_process.cpp"
    )
    target_link_libraries(dc_d47_1_post_process PRIVATE dc_gl)
    add_test(NAME dc_d47_1_post_process COMMAND dc_d47_1_post_process)

    # D47.2 PostProcess blur (GL)
    add_executable(dc_d47_2_blur_pass
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d47_2_blur_pass.cpp"
    )
    target_link_libraries(dc_d47_2_blur_pass PRIVATE dc_gl)
    add_test(NAME dc_d47_2_blur_pass COMMAND dc_d47_2_blur_pass)

    # D56.2 BatchBuilder GL (GL)
    add_executable(dc_d56_2_batch_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d56_2_batch_gl.cpp"
    )
    target_link_libraries(dc_d56_2_batch_gl PRIVATE dc_gl)
    add_test(NAME dc_d56_2_batch_gl COMMAND dc_d56_2_batch_gl)

    # D78.2 BuiltinEffects (GL)
    add_executable(dc_d78_2_builtin_effects
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d78_2_builtin_effects.cpp"
    )
    target_link_libraries(dc_d78_2_builtin_effects PRIVATE dc_gl)
    add_test(NAME dc_d78_2_builtin_effects COMMAND dc_d78_2_builtin_effects)

    # D78.3 Pane borders (GL)
    add_executable(dc_d78_3_pane_borders
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d78_3_pane_borders.cpp"
    )
    target_link_libraries(dc_d78_3_pane_borders PRIVATE dc_gl)
    add_test(NAME dc_d78_3_pane_borders COMMAND dc_d78_3_pane_borders)
  endif()

  # D44.2 AnnotationRenderer GL (needs font + GL)
  if (DC_HAS_GLAD AND DC_HAS_OSMESA AND EXISTS "${DC_TEST_FONT}")
    add_executable(dc_d44_2_annotation_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d44_2_annotation_gl.cpp"
    )
    target_link_libraries(dc_d44_2_annotation_gl PRIVATE dc_gl)
    target_compile_definitions(dc_d44_2_annotation_gl PRIVATE
      FONT_PATH="${DC_TEST_FONT}"
    )
    add_test(NAME dc_d44_2_annotation_gl COMMAND dc_d44_2_annotation_gl)
  endif()

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
  if (DC_HAS_GLAD AND DC_HAS_OSMESA)
    add_executable(dc_d79_2_json_host_gl
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/d79_2_json_host_gl.cpp"
    )
    target_link_libraries(dc_d79_2_json_host_gl PRIVATE dc_gl)
    add_test(NAME dc_d79_2_json_host_gl COMMAND dc_d79_2_json_host_gl)
  endif()
endif()
