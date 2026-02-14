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
  endif()
endif()
