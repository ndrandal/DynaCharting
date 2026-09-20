#!/usr/bin/env bash
# LIMITATIONS.md id guard (ENC-1277).
#
#   bash scripts/check-limitation-ids.sh
#
# Two assertions, both about ids rather than content, because both failures this
# exists for were INVISIBLE in the content:
#
#   1. NO TWO ENTRIES SHARE AN ID. Three collisions landed in two days
#      (ENC-1252/1257 -> DC-L12, ENC-1250/1254/1256 -> DC-L14 three ways,
#      ENC-1251/1253 -> DC-L17). Twice git AUTO-MERGED the two entries into
#      different parts of the file: no conflict, no marker, no error — two
#      headings wearing one number. Only counting headings reveals it.
#
#   2. NO ID DISAPPEARS. Resolving a LIMITATIONS.md conflict with `--ours`
#      wholesale silently dropped ENC-1257's whole DC-L12 entry, including the
#      only record that SvgExporter does not apply the bar-sizing rule. It was
#      caught by a human reading the diff. Entries are append-only (§H device 4:
#      fixed entries move to §R, they are not deleted), so an id present in the
#      baseline and absent here is a dropped entry, every time.
#
# The id scheme itself is §H device 6: the two-digit sequential space DC-L01..DC-L18
# is CLOSED, and every entry added after ENC-1277 is `DC-L-<its ENC ticket number>`,
# which cannot collide because Linear allocates the number.
#
# Exit 0 clean, 1 on a violation, 2 when it could not run (a `could not run` is
# never reported as a pass — DC-L01's whole lesson).
set -uo pipefail

cd "$(dirname "$0")/.." || exit 2
LIM=LIMITATIONS.md
[ -f "$LIM" ] || { printf 'CANNOT RUN: no %s under %s\n' "$LIM" "$PWD"; exit 2; }

# An id is the heading token: legacy two-digit `DC-L07`, or ticket-derived `DC-L-1277`.
ids_from() { grep -oE '^## DC-L-?[0-9]+' | sed 's/^## //'; }

here=$(ids_from < "$LIM")
n=$(printf '%s\n' "$here" | grep -c . )
[ "$n" -gt 0 ] || { printf 'CANNOT RUN: no `## DC-L…` headings in %s — the pattern moved\n' "$LIM"; exit 2; }

fail=0

# --- 1. duplicate ids -------------------------------------------------------
dupes=$(printf '%s\n' "$here" | sort | uniq -d)
if [ -n "$dupes" ]; then
  printf 'FAIL: %s has entries sharing an id:\n' "$LIM"
  while read -r id; do
    [ -n "$id" ] || continue
    printf '  %s appears %s times:\n' "$id" "$(printf '%s\n' "$here" | grep -cx -- "$id")"
    grep -nE "^## ${id} " "$LIM" | sed 's/^/    /'
  done <<< "$dupes"
  printf '  Two entries wearing one id is what ENC-1277 exists to stop. Renumber the\n'
  printf '  NEWER one to DC-L-<its ENC ticket number> and fix every citation of it.\n'
  fail=1
else
  printf 'ok   %s ids are unique (%s entries)\n' "$LIM" "$n"
fi

# --- 2. no id vanishes (append-only) ----------------------------------------
base=""
for ref in origin/main main HEAD; do
  if git rev-parse --verify --quiet "$ref:$LIM" >/dev/null 2>&1; then base="$ref"; break; fi
done
if [ -z "$base" ]; then
  printf 'SKIP no baseline ref resolves (%s not in origin/main, main or HEAD) — the\n' "$LIM"
  printf '     append-only half did NOT run. This is a skip, not a pass.\n'
else
  gone=$(comm -23 \
    <(git show "$base:$LIM" | ids_from | sort -u) \
    <(printf '%s\n' "$here" | sort -u))
  if [ -n "$gone" ]; then
    printf 'FAIL: ids present in %s but missing here — an entry was dropped, not retired:\n' "$base"
    printf '%s\n' "$gone" | sed 's/^/    /'
    printf '  Retiring an entry keeps its heading and moves it to §R (§H device 4). A\n'
    printf '  vanished id is the `--ours` conflict resolution that ate ENC-1257 once.\n'
    fail=1
  else
    printf 'ok   no id present in %s has vanished (append-only holds)\n' "$base"
  fi
fi

# --- 3. ids are well formed -------------------------------------------------
bad=$(grep -nE '^## DC-L' "$LIM" | grep -vE '^[0-9]+:## DC-L(-[0-9]{3,}|[0-9]{2}) ' || true)
if [ -n "$bad" ]; then
  printf 'FAIL: heading(s) whose id is neither a legacy `DC-Lnn` nor `DC-L-<ticket>`:\n'
  printf '%s\n' "$bad" | sed 's/^/    /'
  fail=1
else
  printf 'ok   every id is a closed-space DC-Lnn or a ticket-derived DC-L-<ticket>\n'
fi

exit "$fail"
