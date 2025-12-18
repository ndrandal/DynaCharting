cmake_minimum_required(VERSION 3.20)
project(dc_core LANGUAGES CXX)

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
# Core library
# =======================
file(GLOB_RECURSE DC_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE DC_PUBLIC_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(dc STATIC ${DC_SOURCES} ${DC_PUBLIC_HEADERS})
target_include_directories(dc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
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
# D1.1 verification (smoke)
# =======================
if (DC_BUILD_TESTS)
  enable_testing()

  add_executable(dc_d1_1_smoke
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/d1_1_smoke.cpp"
  )
  target_link_libraries(dc_d1_1_smoke PRIVATE dc)

  add_test(NAME dc_d1_1_smoke COMMAND dc_d1_1_smoke)
endif()
