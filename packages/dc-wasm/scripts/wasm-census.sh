#!/usr/bin/env bash
# wasm-census.sh — ENC-1112. What is ACTUALLY in the committed
# packages/dc-wasm/wasm/dc_engine_host.wasm, measured from the artifact rather
# than inferred from core/CMakeLists.txt.
#
#   bash packages/dc-wasm/scripts/wasm-census.sh                  # full census
#   bash packages/dc-wasm/scripts/wasm-census.sh EncodePass Recipe # ask about symbols
#
# WHY THIS EXISTS, AND WHY `strings` IS NOT IT
# --------------------------------------------
# The committed .wasm carries no `name` section, so it has no symbol table and
# `strings` can only see STRING LITERALS. That makes `strings … | grep -ci <thing>`
# an EMBIND-NAME test, not a code-presence test, and it is wrong in both
# directions (LIMITATIONS.md DC-L-1112, §C7):
#
#   dc::sceneToDocument        strings=0  but IS in the module
#   "encode"                   strings=25 and NO encode-pass code is in the module
#                              (all 25 are wgpu*CommandEncoder* import names)
#
# So this script rebuilds the SAME link with `--profiling-funcs`, which adds a
# `name` section and nothing else, and then PROVES that is what it did: strip the
# name section back off and the bytes must equal the committed artifact. If they
# do not, the census is refused (exit 2) rather than reported — a measurement of
# something other than the shipped bytes is not a measurement.
#
# Exit 0 clean, 1 if a queried symbol is ABSENT from the module, 2 when it could
# not run. A `could not run` is never reported as a pass (DC-L01's lesson).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$PKG_DIR/../.." && pwd)"
BUILD_DIR="$ROOT/build-wasm"
SHIPPED="$PKG_DIR/wasm/dc_engine_host.wasm"

die2() { printf 'CANNOT RUN: %s\n' "$1" >&2; exit 2; }

command -v em++ >/dev/null 2>&1 || die2 "em++ not on PATH — source ~/emsdk/emsdk_env.sh (see CLAUDE.md)"
command -v ninja >/dev/null 2>&1 || die2 "ninja not on PATH"
EMBIN="$(dirname "$(command -v em++)")/../bin"
NM="$EMBIN/llvm-nm"; OBJCOPY="$EMBIN/llvm-objcopy"; OBJDUMP="$EMBIN/llvm-objdump"
for t in "$NM" "$OBJCOPY" "$OBJDUMP"; do [ -x "$t" ] || die2 "missing emsdk tool $t"; done
[ -f "$SHIPPED" ] || die2 "no committed artifact at $SHIPPED"
[ -f "$BUILD_DIR/build.ninja" ] || die2 "no $BUILD_DIR — run: bash packages/dc-wasm/scripts/build-wasm.sh"

# The link command is taken from ninja rather than restated here, so this script
# cannot drift from core/CMakeLists.txt's dc_engine_host target.
LINK="$(cd "$BUILD_DIR" && ninja -t commands core/dc_engine_host.js 2>/dev/null | tail -1)"
case "$LINK" in
  *dc_engine_host.js*) ;;
  *) die2 "could not read the dc_engine_host link command out of $BUILD_DIR/build.ninja" ;;
esac
LINK="${LINK#: && }"; LINK="${LINK% && :}"

d="$(mktemp -d "${TMPDIR:-/tmp}/dc-wasm-census.XXXXXX")" || die2 "mktemp failed"
trap 'rm -rf "$d"' EXIT

PROBE_LINK="${LINK/-o core\/dc_engine_host.js/--profiling-funcs -Wl,--why-extract=$d/why.txt -Wl,--Map=$d/link.map -o $d/dc_engine_host.js}"
[ "$PROBE_LINK" != "$LINK" ] || die2 "could not rewrite the link command's -o (it moved)"

( cd "$BUILD_DIR" && eval "$PROBE_LINK" ) >"$d/link.log" 2>&1 \
  || { sed -n '1,20p' "$d/link.log" >&2; die2 "the probe relink failed"; }

# --- the admissibility gate: the probe module IS the shipped artifact ---------
"$OBJCOPY" --remove-section=name "$d/dc_engine_host.wasm" "$d/stripped.wasm" 2>/dev/null \
  || die2 "llvm-objcopy could not strip the name section"
a="$(sha256sum "$d/stripped.wasm" | cut -d' ' -f1)"
b="$(sha256sum "$SHIPPED"         | cut -d' ' -f1)"
if [ "$a" != "$b" ]; then
  die2 "the name-stripped probe does not equal the committed artifact
  probe     $a
  committed $b
  Either $BUILD_DIR is stale against the working tree, or the committed .wasm is
  stale against the source. Rebuild (bash packages/dc-wasm/scripts/build-wasm.sh)
  and re-run. The census is REFUSED rather than reported — it would describe
  bytes nobody ships."
fi
printf 'ok   probe module == committed artifact after stripping `name` (sha256 %s…)\n' "${b:0:16}"

"$NM" --defined-only "$d/dc_engine_host.wasm" 2>/dev/null \
  | sed -E 's/^[0-9a-f]+ [a-zA-Z] //' > "$d/defined.txt"
nfun=$(grep -c . "$d/defined.txt")
[ "$nfun" -gt 1000 ] || die2 "only $nfun names in the probe module — --profiling-funcs did not take"

awk -F'\t' 'NR>1{print $2}' "$d/why.txt" | grep 'libdc\.a' \
  | sed 's/.*libdc\.a(//; s/)$//' | sort -u > "$d/extracted.txt"
"$EMBIN/llvm-ar" t "$BUILD_DIR/core/libdc.a" 2>/dev/null | sort -u > "$d/members.txt"

printf '\n=== dc_engine_host.wasm — measured census ===\n'
printf '  defined wasm functions          %s\n' "$nfun"
printf '  libdc.a translation units       %s total, %s extracted into the link\n' \
  "$(grep -c . "$d/members.txt")" "$(grep -c . "$d/extracted.txt")"
printf '  (extraction is necessary, not sufficient: --gc-sections runs AFTER it)\n'

# Per-TU function counts come from the LINK MAP, which names the owning archive
# member of every function wasm-ld emitted. A name-substring count would be a
# guess (and would credit `SceneDocument.cpp.o` with the `DocPane` helpers that
# `Scene.cpp.o` actually emitted).
printf '\n--- libdc.a TUs in the module, by functions emitted ---------------------\n'
while read -r m; do
  n=$(grep -cF "libdc.a($m):" "$d/link.map")
  printf '  %-30s %6s function(s)%s\n' "$m" "$n" \
    "$([ "$n" -eq 0 ] && printf '   <- extracted, then entirely --gc-sections-ed' )"
done < "$d/extracted.txt"

# gpu/ and host/ are deliberately NOT libdc.a members (core/CMakeLists.txt filters
# them out of the dc glob): gpu/ is compiled straight into dc_engine_host, host/
# is the standalone dc_json_host. Neither can appear in an archive-extraction
# list, so neither belongs in this one.
printf '\n--- core/src subdirectories with ZERO code in the module ----------------\n'
for sub in "$ROOT"/core/src/*/; do
  name="${sub%/}"; name="${name##*/}"
  case "$name" in gpu|host) continue ;; esac
  tus=0; present=0
  for f in "$sub"*.cpp; do
    [ -f "$f" ] || continue
    tus=$((tus+1))
    b="$(basename "$f" .cpp)"
    [ "$(grep -cF "libdc.a($b.cpp.o):" "$d/link.map")" -gt 0 ] && present=$((present+1))
  done
  [ "$tus" -gt 0 ] && [ "$present" -eq 0 ] && printf '  %-24s %s TU(s), no code in the module\n' "core/src/$name" "$tus"
done

rc=0
if [ "$#" -gt 0 ]; then
  printf '\n--- queried symbols -----------------------------------------------------\n'
  for q in "$@"; do
    # Anchored at the END so a prefix cannot answer for its own extension:
    # `dc::serializeScene` must not be satisfied by `dc::serializeSceneDocument`
    # (the same trap as DC-L12 vs DC-L-1277 one file over).
    n=$(grep -cE -- "${q}([^A-Za-z0-9_]|$)" "$d/defined.txt")
    s=$(strings "$SHIPPED" | grep -cF -- "$q")
    if [ "$n" -gt 0 ]; then
      printf '  PRESENT  %-28s %4s function(s) in the module   (strings says %s)\n' "$q" "$n" "$s"
    else
      printf '  ABSENT   %-28s    0 functions in the module    (strings says %s)\n' "$q" "$s"
      rc=1
    fi
  done
  printf '  (a `strings` count that disagrees with the verdict is `strings` being wrong —\n'
  printf '   it sees string literals, not code. LIMITATIONS.md DC-L-1112 / §C7.)\n'
fi
exit $rc
