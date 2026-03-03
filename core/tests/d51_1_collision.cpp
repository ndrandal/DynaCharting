// D51.1 — CollisionSolver: two overlapping labels pushed apart after solve
#include "dc/layout/CollisionSolver.hpp"

#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static bool near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) < eps;
}

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D51.1 CollisionSolver Tests ===\n");

  dc::CollisionSolver solver;

  // Test 1: Two labels at exact same position — must be pushed apart
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 50.0f, 20.0f, 0, 0, 0, false, true},
      {10.0f, 10.0f, 50.0f, 20.0f, 0, 0, 0, false, true}
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 2.0f;
    cfg.maxIterations = 10;

    auto result = solver.solve(labels, cfg);
    check(result.labels.size() == 2, "two labels in result");

    float dx = result.labels[0].x - result.labels[1].x;
    float dy = result.labels[0].y - result.labels[1].y;
    float dist = std::sqrt(dx * dx + dy * dy);
    check(dist > 1.0f, "labels separated after solve");
  }

  // Test 2: Non-overlapping labels — should not move
  {
    std::vector<dc::LabelRect> labels = {
      {0.0f, 0.0f, 30.0f, 10.0f, 0, 0, 0, false, true},
      {100.0f, 100.0f, 30.0f, 10.0f, 0, 0, 0, false, true}
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 2.0f;

    auto result = solver.solve(labels, cfg);
    check(near(result.labels[0].x, 0.0f) && near(result.labels[0].y, 0.0f),
          "first label unmoved");
    check(near(result.labels[1].x, 100.0f) && near(result.labels[1].y, 100.0f),
          "second label unmoved");
    check(result.iterations == 0, "zero iterations for non-overlapping");
  }

  // Test 3: Three overlapping labels — all separated
  {
    std::vector<dc::LabelRect> labels = {
      {0.0f, 0.0f, 40.0f, 15.0f, 0, 0, 0, false, true},
      {5.0f, 2.0f, 40.0f, 15.0f, 0, 0, 0, false, true},
      {10.0f, 4.0f, 40.0f, 15.0f, 0, 0, 0, false, true}
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 1.0f;
    cfg.maxIterations = 20;

    auto result = solver.solve(labels, cfg);

    // Verify no pair overlaps (without padding for simplicity — just check separation)
    bool anyOverlap = false;
    for (std::size_t i = 0; i < result.labels.size() && !anyOverlap; ++i) {
      for (std::size_t j = i + 1; j < result.labels.size(); ++j) {
        auto& a = result.labels[i];
        auto& b = result.labels[j];
        bool ox = (a.x < b.x + b.width) && (a.x + a.width > b.x);
        bool oy = (a.y < b.y + b.height) && (a.y + a.height > b.y);
        if (ox && oy) { anyOverlap = true; break; }
      }
    }
    check(!anyOverlap, "three labels: no overlapping pairs after solve");
  }

  // Test 4: Padding respected
  {
    std::vector<dc::LabelRect> labels = {
      {0.0f, 0.0f, 20.0f, 10.0f, 0, 0, 0, false, true},
      {21.0f, 0.0f, 20.0f, 10.0f, 0, 0, 0, false, true}  // 1px gap, but padding=2
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 2.0f;

    auto result = solver.solve(labels, cfg);
    float gap = result.labels[1].x - (result.labels[0].x + result.labels[0].width);
    // With padding=2, gap might be small but labels should have been pushed
    // One of the labels should have moved
    float moved0 = std::fabs(result.labels[0].x - 0.0f) + std::fabs(result.labels[0].y - 0.0f);
    float moved1 = std::fabs(result.labels[1].x - 21.0f) + std::fabs(result.labels[1].y - 0.0f);
    check(moved0 > 0.1f || moved1 > 0.1f, "at least one label moved due to padding");
  }

  // Test 5: Fixed label doesn't move
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 50.0f, 20.0f, 0, 0, 0, true, true},   // fixed
      {10.0f, 10.0f, 50.0f, 20.0f, 0, 0, 0, false, true}    // movable
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 0.0f;

    auto result = solver.solve(labels, cfg);
    // Find the fixed label (sort by priority may reorder, but both have priority 0)
    const dc::LabelRect* fixedLabel = nullptr;
    const dc::LabelRect* movedLabel = nullptr;
    for (auto& l : result.labels) {
      if (l.fixed) fixedLabel = &l;
      else movedLabel = &l;
    }
    check(fixedLabel != nullptr, "fixed label found in result");
    check(near(fixedLabel->x, 10.0f) && near(fixedLabel->y, 10.0f),
          "fixed label unmoved");
    check(movedLabel != nullptr, "movable label found in result");
    float movedDist = std::sqrt(
      (movedLabel->x - 10.0f) * (movedLabel->x - 10.0f) +
      (movedLabel->y - 10.0f) * (movedLabel->y - 10.0f));
    check(movedDist > 1.0f, "movable label displaced");
  }

  // Test 6: Empty input
  {
    std::vector<dc::LabelRect> labels;
    auto result = solver.solve(labels, {});
    check(result.labels.empty(), "empty input returns empty result");
    check(result.iterations == 0, "zero iterations for empty input");
  }

  // Test 7: Single label — unchanged
  {
    std::vector<dc::LabelRect> labels = {
      {5.0f, 5.0f, 30.0f, 12.0f, 0, 0, 0, false, true}
    };

    auto result = solver.solve(labels, {});
    check(result.labels.size() == 1, "single label returned");
    check(near(result.labels[0].x, 5.0f), "single label x unchanged");
    check(near(result.labels[0].y, 5.0f), "single label y unchanged");
  }

  std::printf("=== D51.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
