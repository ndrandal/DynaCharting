#!/usr/bin/env bash
# ENC-1249 — the tier-0 ("Truthful") check, in ONE command.
#
#   bash scripts/tier0.sh [build-dir]        # default build dir: build-dawn
#
# specs/2026-09-19-chart-quality-bar/SPEC.md D1 defines tier 0 as "the mark depicts
# the data" and its falsifiable check as: render a known-answer synthetic series and
# assert on pixels. This script runs that check AND its two negative controls, so
# every run re-demonstrates that the check can fail. A check never seen to fail is
# not a check.
#
#   1. POSITIVE   honest data, presented raster          -> must exit 0
#   2. NEGATIVE   --invert-data   (a descending ramp;    -> must exit NON-ZERO
#                 candle body/wick extents swapped)
#   3. NEGATIVE   --invert-render (the raw, unflipped    -> must exit NON-ZERO
#                 readback: LIMITATIONS.md DC-L05)
#
# Exit codes: 0 all three behaved; 1 a verdict was wrong; 2 the check COULD NOT RUN.
# 2 is never silently treated as success — that is DC-L01's whole lesson.
set -u -o pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${REPO_ROOT}/build-dawn}"
case "${BUILD_DIR}" in /*) ;; *) BUILD_DIR="${REPO_ROOT}/${BUILD_DIR}" ;; esac
BIN="${BUILD_DIR}/core/dc_enc1249_tier0_truthful"

# Per-run temp dir. /tmp is shared between parallel agents and a fixed name gets
# clobbered mid-run (workspace CLAUDE.md, ENC-1222/ENC-1185).
TMPD="$(mktemp -d "${TMPDIR:-/tmp}/enc1249-tier0.XXXXXX")"
trap 'rm -rf "${TMPD}"' EXIT

if [ ! -x "${BIN}" ]; then
  cat >&2 <<EOF
CANNOT RUN: ${BIN} not found.

Tier 0 is a claim about PIXELS, so it needs the renderer, and the renderer is
excluded from the default build at configure time (LIMITATIONS.md DC-L01).
Build it once (~30-60 min for Dawn from source, then incremental):

  cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
  cmake --build build-dawn -j\$(nproc) --target dc_enc1249_tier0_truthful
EOF
  exit 2
fi

# A headless box often has no hardware Vulkan adapter; lavapipe is the documented
# fallback. Only set it if the caller did not, so a deliberate adapter choice wins.
if [ -z "${VK_ICD_FILENAMES:-}" ] && \
   [ -f /usr/share/vulkan/icd.d/lvp_icd.x86_64.json ]; then
  export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
fi

run() {  # run <label> <logfile> [args...]
  local label="$1" log="$2"; shift 2
  printf '\n\033[1m=== %s ===\033[0m\n' "${label}"
  "${BIN}" "$@" >"${log}" 2>&1
  local rc=$?
  cat "${log}"
  return ${rc}
}

verdict=0

run "1/3 POSITIVE — honest data" "${TMPD}/pos.log"
pos=$?
if [ ${pos} -eq 3 ]; then
  echo >&2
  echo "CANNOT RUN: no Dawn adapter. See the hint above." >&2
  exit 2
fi

run "2/3 NEGATIVE CONTROL — deliberately wrong DATA" "${TMPD}/neg-data.log" --invert-data
negd=$?

run "3/3 NEGATIVE CONTROL — raw (unflipped) readback" "${TMPD}/neg-render.log" --invert-render
negr=$?

printf '\n\033[1m=== tier 0 verdict ===\033[0m\n'

say() {  # say <name> <expected> <got> <ok>
  if [ "$4" = "1" ]; then
    printf '  OK    %-46s expected %-8s got exit %s\n' "$1" "$2" "$3"
  else
    printf '  WRONG %-46s expected %-8s got exit %s\n' "$1" "$2" "$3"
    verdict=1
  fi
}

if [ ${pos} -eq 0 ]; then say "positive (honest data)" "exit 0" "${pos}" 1
else say "positive (honest data)" "exit 0" "${pos}" 0; fi
if [ ${negd} -ne 0 ]; then say "negative control: --invert-data" "non-zero" "${negd}" 1
else say "negative control: --invert-data" "non-zero" "${negd}" 0; fi
if [ ${negr} -ne 0 ]; then say "negative control: --invert-render" "non-zero" "${negr}" 1
else say "negative control: --invert-render" "non-zero" "${negr}" 0; fi

if [ ${verdict} -eq 0 ]; then
  printf '\n  TIER 0 PASSES, and was demonstrated FAILING on both wrong inputs.\n'
else
  printf '\n  TIER 0 VERDICT IS WRONG.\n'
  if [ ${negd} -eq 0 ] || [ ${negr} -eq 0 ]; then
    printf '  A negative control PASSED: the check cannot fail, so it is not a check.\n'
  fi
fi
exit ${verdict}
