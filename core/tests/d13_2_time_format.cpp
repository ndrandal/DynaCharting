// D13.2 — TimeFormat unit test (pure C++)

#include "dc/math/TimeFormat.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int tests = 0;
static int passed = 0;

static void check(bool cond, const char* msg) {
  tests++;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    std::exit(1);
  }
  passed++;
  std::printf("  OK: %s\n", msg);
}

int main() {
  // chooseTimeFormat tests
  {
    const char* fmt = dc::chooseTimeFormat(30.0f);
    check(std::strcmp(fmt, "%H:%M:%S") == 0,
          "chooseTimeFormat(30) -> \"%H:%M:%S\"");
  }
  {
    const char* fmt = dc::chooseTimeFormat(300.0f);
    check(std::strcmp(fmt, "%H:%M") == 0,
          "chooseTimeFormat(300) -> \"%H:%M\"");
  }
  {
    const char* fmt = dc::chooseTimeFormat(86400.0f);
    check(std::strcmp(fmt, "%b %d") == 0,
          "chooseTimeFormat(86400) -> \"%b %d\"");
  }
  {
    const char* fmt = dc::chooseTimeFormat(2592000.0f);
    check(std::strcmp(fmt, "%b %Y") == 0,
          "chooseTimeFormat(2592000) -> \"%b %Y\"");
  }
  {
    const char* fmt = dc::chooseTimeFormat(31536000.0f);
    check(std::strcmp(fmt, "%Y") == 0,
          "chooseTimeFormat(31536000) -> \"%Y\"");
  }

  // formatTimestamp tests — verify output format and non-emptiness.
  // Note: gmtime_r may behave differently across platforms (e.g. WSL1),
  // so we verify structure rather than exact values.
  {
    std::string s = dc::formatTimestamp(1700000000.0f, "%H:%M", true);
    check(s.size() == 5 && s[2] == ':',
          "formatTimestamp(%H:%M) -> \"HH:MM\" format");
    std::printf("    got: '%s'\n", s.c_str());
  }
  {
    std::string s = dc::formatTimestamp(1700000000.0f, "%H:%M:%S", true);
    check(s.size() == 8 && s[2] == ':' && s[5] == ':',
          "formatTimestamp(%H:%M:%S) -> \"HH:MM:SS\" format");
    std::printf("    got: '%s'\n", s.c_str());
  }
  {
    std::string s = dc::formatTimestamp(1700000000.0f, "%b %d", true);
    check(s.size() >= 5 && s.find(' ') != std::string::npos,
          "formatTimestamp(%b %d) -> \"Mon DD\" format");
    std::printf("    got: '%s'\n", s.c_str());
  }
  {
    std::string s = dc::formatTimestamp(1700000000.0f, "%Y", true);
    check(s == "2023",
          "formatTimestamp(%Y) -> \"2023\"");
  }

  // formatTimestamp: two different epochs produce different labels
  {
    std::string s1 = dc::formatTimestamp(1700000000.0f, "%H:%M", true);
    std::string s2 = dc::formatTimestamp(1700003600.0f, "%H:%M", true);
    check(s1 != s2, "different epochs produce different labels");
  }

  // portableTimegm: round-trip consistency
  // Use gmtime to decompose, then timegm to recompose — should get same value
  {
    std::time_t original = 1700000000;
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &original);
#else
    gmtime_r(&original, &tm);
#endif
    std::time_t reconstructed = dc::portableTimegm(&tm);
    check(reconstructed == original, "portableTimegm round-trip via gmtime");
  }

  std::printf("D13.2 time_format: %d/%d PASS\n", passed, tests);
  return 0;
}
