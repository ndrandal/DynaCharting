// ENC-982: DC_CHECK — an assertion that survives -DNDEBUG (i.e. Release builds).
//
// WHY: `assert()` is compiled out under NDEBUG, which CMAKE_BUILD_TYPE=Release
// adds. Any test whose only failure signal is `assert()` therefore passes
// vacuously in Release. Worse, an `assert()` whose expression has SIDE EFFECTS
// (e.g. `assert(parseSceneDocument(json, doc))` — `doc` is an out-param) stops
// doing the work entirely, so later lookups operate on empty state.
//
// DC_CHECK is unconditional: the expression is ALWAYS evaluated, and on failure
// it prints `expr` + file:line + function to stderr and exits non-zero, which is
// what ctest keys off. It mirrors the `exit(1)` / `check()` convention already
// used by the other ~218 logic tests in this directory, and keeps assert()'s
// abort-on-first-failure semantics (important: once a parse has failed there is
// no point evaluating the assertions that read the parsed result).
//
// The macro is variadic so expressions containing top-level commas —
// `DC_CHECK(feq(a, b))`, template argument lists — pass through intact.

#ifndef DC_TESTS_DC_CHECK_HPP
#define DC_TESTS_DC_CHECK_HPP

#include <cstdio>
#include <cstdlib>

namespace dc {
namespace test {

inline void dcCheckFailed(const char* expr, const char* file, int line, const char* func) {
  std::fflush(stdout);
  std::fprintf(stderr, "  FAIL: DC_CHECK(%s)\n    at %s:%d in %s()\n", expr, file, line, func);
  std::fflush(stderr);
  std::exit(1);
}

}  // namespace test
}  // namespace dc

#define DC_CHECK_STRINGIFY_(...) #__VA_ARGS__

// DC_CHECK(expr...) — always evaluated, never removed by NDEBUG.
#define DC_CHECK(...)                                                                  \
  do {                                                                                 \
    if (!(__VA_ARGS__)) {                                                              \
      ::dc::test::dcCheckFailed(DC_CHECK_STRINGIFY_(__VA_ARGS__), __FILE__, __LINE__,  \
                                __func__);                                             \
    }                                                                                  \
  } while (0)

// DC_CHECK_CONTAINS(map, key) — checked associative lookup.
// Use INSTEAD OF `container.at(key)` in tests: a missing key reports a readable
// DC_CHECK failure with the expression and location rather than escaping the
// test as an uncaught `std::out_of_range: map::at` (which is what the two
// Release failures in d79_1/d80_1 looked like before ENC-982).
#define DC_CHECK_CONTAINS(container, key) DC_CHECK((container).count(key) == 1)

#endif  // DC_TESTS_DC_CHECK_HPP
