// ENC-627 (C1) — PickInstanceTable: the per-DrawItem row-id side table that maps
// a (drawItemId, instanceIndex) pick to a durable source row id. Pure dc; the
// ENC-628 shader change supplies the instance index that makes this resolve a
// real id at pick time.
#include "dc/render/PickInstanceTable.hpp"

#include <cstdio>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

using namespace dc;

int main() {
  std::printf("=== ENC-627 (C1) PickInstanceTable ===\n");
  PickInstanceTable t;

  const Id kRect = 10, kScatter = 20;
  check(!t.has(kRect) && t.size() == 0, "empty table");
  check(t.rowIdForInstance(kRect, 0) == -1, "unknown draw item -> -1");

  // Register a treemap's per-instance row ids (instance order).
  t.setInstanceRowIds(kRect, {100, 101, 102, 103});
  check(t.has(kRect) && t.size() == 1, "registered rect ids");
  check(t.rowIdForInstance(kRect, 0) == 100, "instance 0 -> row 100");
  check(t.rowIdForInstance(kRect, 3) == 103, "instance 3 -> row 103");
  check(t.rowIdForInstance(kRect, 4) == -1, "out-of-range instance -> -1");
  check(t.rowIdForInstance(kRect, -1) == -1, "negative instance (unknown) -> -1");

  // A second DrawItem is independent.
  t.setInstanceRowIds(kScatter, {7, 8});
  check(t.size() == 2, "two draw items");
  check(t.rowIdForInstance(kScatter, 1) == 8, "scatter instance 1 -> row 8");
  check(t.rowIdForInstance(kRect, 1) == 101, "rect unaffected by scatter");

  // Re-register replaces.
  t.setInstanceRowIds(kRect, {200, 201});
  check(t.rowIdForInstance(kRect, 0) == 200 && t.rowIdForInstance(kRect, 2) == -1,
        "re-register replaces ids (and shrinks)");

  // Empty vector clears the entry.
  t.setInstanceRowIds(kRect, {});
  check(!t.has(kRect), "empty ids clears entry");

  // remove / clear.
  t.remove(kScatter);
  check(t.size() == 0, "remove clears the last entry");
  t.setInstanceRowIds(kRect, {1});
  t.clear();
  check(t.size() == 0, "clear empties the table");

  // instanceRowIds view.
  t.setInstanceRowIds(kScatter, {5, 6, 7});
  check(t.instanceRowIds(kScatter).size() == 3, "instanceRowIds view size");
  check(t.instanceRowIds(999).empty(), "unknown view is empty");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
