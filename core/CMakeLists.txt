cmake_minimum_required(VERSION 3.20)
project(dc_core LANGUAGES C CXX)

# ---- Options ----
option(DC_BUILD_TESTS "Build core tests in core/tests/" ON)
option(DC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

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
  endif()
endif()
