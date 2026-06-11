#!/usr/bin/env bash
# build-wasm.sh — ENC-506 (P6.5) reproducible build of the @repo/dc-wasm WASM
# artifacts (dc_engine_host.js + dc_engine_host.wasm) from the C++ core.
#
# Builds the EMSCRIPTEN-gated `dc_engine_host` CMake target (core/CMakeLists.txt)
# and copies the .js + .wasm into packages/dc-wasm/wasm/. EMSCRIPTEN-gated +
# additive: the native `dc` build is unaffected.
#
# Prerequisites:
#   * Emscripten on PATH (source ~/emsdk/emsdk_env.sh).
#   * Ninja + a C++20-capable clang (provided by emsdk).
#   * RapidJSON headers under third_party/rapidjson/include (header-only). They
#     are gitignored; provision locally with:
#       git clone --depth 1 https://github.com/Tencent/rapidjson.git \
#         third_party/rapidjson
#
# Usage:  bash packages/dc-wasm/scripts/build-wasm.sh
set -euo pipefail

# Resolve the DynaCharting repo root (two levels up from this script's package).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$PKG_DIR/../.." && pwd)"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "emcmake not found — run: source ~/emsdk/emsdk_env.sh" >&2
  exit 1
fi

RJ="$ROOT/third_party/rapidjson/include"
if [ ! -f "$RJ/rapidjson/document.h" ]; then
  echo "RapidJSON headers missing at $RJ" >&2
  echo "  git clone --depth 1 https://github.com/Tencent/rapidjson.git $ROOT/third_party/rapidjson" >&2
  exit 1
fi

BUILD_DIR="$ROOT/build-wasm"

echo "[dc-wasm] configuring ($BUILD_DIR) …"
emcmake cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja \
  -DTHIRD_PARTY_ROOT="$ROOT/third_party" \
  -DRAPIDJSON_INCLUDE_DIR="$RJ" \
  -DDC_BUILD_TESTS=OFF

echo "[dc-wasm] building dc_engine_host …"
cmake --build "$BUILD_DIR" --target dc_engine_host -j"$(nproc)"

echo "[dc-wasm] copying artifacts -> $PKG_DIR/wasm/ …"
mkdir -p "$PKG_DIR/wasm"
cp "$BUILD_DIR/core/dc_engine_host.js"   "$PKG_DIR/wasm/"
cp "$BUILD_DIR/core/dc_engine_host.wasm" "$PKG_DIR/wasm/"

echo "[dc-wasm] done:"
ls -la "$PKG_DIR/wasm/"
