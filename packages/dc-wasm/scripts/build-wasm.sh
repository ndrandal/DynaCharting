#!/usr/bin/env bash
# build-wasm.sh — ENC-506 (P6.5) reproducible build of the @repo/dc-wasm WASM
# artifacts (dc_engine_host.js + dc_engine_host.wasm) from the C++ core.
#
# Builds the EMSCRIPTEN-gated `dc_engine_host` CMake target (core/CMakeLists.txt)
# and copies the .js + .wasm into packages/dc-wasm/wasm/. EMSCRIPTEN-gated +
# additive: the native `dc` build is unaffected.
#
# Prerequisites (ENC-989): Emscripten on PATH. One-time, no sudo, installs
# entirely under $HOME:
#
#   git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
#   cd ~/emsdk && ./emsdk install 6.0.9 && ./emsdk activate 6.0.9
#   source ~/emsdk/emsdk_env.sh          # once per shell
#
# RapidJSON is provisioned automatically by this script (it is a git submodule,
# which `git worktree add` does not populate). Ninja and a C++20 clang come from
# emsdk. Nothing else is required — if this script fails on a clean checkout with emsdk sourced, that
# is a bug in the script, not in your setup.
#
# Usage:  bash packages/dc-wasm/scripts/build-wasm.sh
set -euo pipefail

# --- pinned toolchain -------------------------------------------------------
# Both are pinned because the artifacts under packages/dc-wasm/wasm/ are
# COMMITTED. An unpinned toolchain makes a rebuild produce a diff for reasons
# unrelated to any source change, which is how a toolchain bump rides into an
# unrelated PR unnoticed (the same failure ENC-1059 fixed for treaty's codegen).
# Bumping either is deliberate: change it here, rebuild, and commit the churn
# on its own.
# RapidJSON's pin is NOT repeated here: it is a git submodule, so the commit
# lives in the gitlink and .gitmodules. Duplicating it in this script would be a
# second source of truth that silently drifts from the first.
EMSDK_VERSION="6.0.9"

# Resolve the DynaCharting repo root (two levels up from this script's package).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$PKG_DIR/../.." && pwd)"

if ! command -v emcmake >/dev/null 2>&1; then
  cat >&2 <<EOM
emcmake not found. Emscripten is not on PATH.

If emsdk is already installed:
  source ~/emsdk/emsdk_env.sh

If not (one-time, no sudo, installs under \$HOME):
  git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
  cd ~/emsdk && ./emsdk install ${EMSDK_VERSION} && ./emsdk activate ${EMSDK_VERSION}
  source ~/emsdk/emsdk_env.sh
EOM
  exit 1
fi

# Warn — do not fail — on a version mismatch. A different emcc still builds a
# working module, but it stamps different bytes into the committed artifacts,
# so the rebuild stops being reproducible and the diff is noise.
ACTUAL_EMCC="$(emcc --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 || true)"
if [ -n "$ACTUAL_EMCC" ] && [ "$ACTUAL_EMCC" != "$EMSDK_VERSION" ]; then
  echo "[dc-wasm] WARNING: emcc is $ACTUAL_EMCC, this script pins $EMSDK_VERSION." >&2
  echo "[dc-wasm]          The build will work, but the committed artifacts under" >&2
  echo "[dc-wasm]          packages/dc-wasm/wasm/ will differ for toolchain reasons." >&2
  echo "[dc-wasm]          Install the pinned version: cd ~/emsdk && ./emsdk install ${EMSDK_VERSION} && ./emsdk activate ${EMSDK_VERSION}" >&2
fi

# RapidJSON is a git submodule, so a fresh clone or a NEW GIT WORKTREE does not
# have its contents — `git worktree add` does not populate submodules. Telling
# the developer to go and provision it by hand is what made "rebuilds from a
# documented command" untrue (ENC-989); do it here instead. The commit comes
# from the gitlink, so this cannot drift from what the repo pins.
RJ_DIR="$ROOT/third_party/rapidjson"
RJ="$RJ_DIR/include"
if [ ! -f "$RJ/rapidjson/document.h" ]; then
  echo "[dc-wasm] RapidJSON headers missing — initialising the submodule …"
  git -C "$ROOT" submodule update --init third_party/rapidjson
fi
if [ ! -f "$RJ/rapidjson/document.h" ]; then
  echo "RapidJSON headers still missing at $RJ after submodule init." >&2
  echo "  try: git -C $ROOT submodule update --init --force third_party/rapidjson" >&2
  exit 1
fi

BUILD_DIR="$ROOT/build-wasm"

echo "[dc-wasm] configuring ($BUILD_DIR) …"
# -ffile-prefix-map rewrites the absolute source path baked into the module by
# __FILE__ (RapidJSON's asserts are the main source of these) to a relative one.
# Without it the artifact depends on WHERE the repo is checked out: building the
# same commit with the same toolchain from two different directories produced
# two different .wasm files, differing only by the length of the embedded path.
# Since the artifacts are committed, that would make a rebuild show a spurious
# diff for every developer whose checkout path differs from the last builder's.
emcmake cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja \
  -DTHIRD_PARTY_ROOT="$ROOT/third_party" \
  -DRAPIDJSON_INCLUDE_DIR="$RJ" \
  -DCMAKE_CXX_FLAGS="-ffile-prefix-map=$ROOT=." \
  -DDC_BUILD_TESTS=OFF

echo "[dc-wasm] building dc_engine_host …"
cmake --build "$BUILD_DIR" --target dc_engine_host -j"$(nproc)"

echo "[dc-wasm] copying artifacts -> $PKG_DIR/wasm/ …"
mkdir -p "$PKG_DIR/wasm"
cp "$BUILD_DIR/core/dc_engine_host.js"   "$PKG_DIR/wasm/"
cp "$BUILD_DIR/core/dc_engine_host.wasm" "$PKG_DIR/wasm/"

echo "[dc-wasm] done:"
ls -la "$PKG_DIR/wasm/"
