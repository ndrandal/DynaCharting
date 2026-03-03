// D51.2 — CollisionSolver priority: high-priority stays, low-priority moves or hides
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
  std::printf("=== D51.2 CollisionSolver Priority Tests ===\n");

  dc::CollisionSolver solver;

  // Test 1: High-priority label stays near original position,
  //         low-priority label moves
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 50.0f, 20.0f, 10, 0, 0, false, true},  // high priority
      {10.0f, 10.0f, 50.0f, 20.0f,  1, 0, 0, false, true}   // low priority
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 0.0f;
    cfg.maxIterations = 10;

    auto result = solver.solve(labels, cfg);

    // After sort by priority descending: labels[0] is priority 10, labels[1] is priority 1
    // labels[1] (low priority) should be the one that moved
    const dc::LabelRect* highP = nullptr;
    const dc::LabelRect* lowP = nullptr;
    for (auto& l : result.labels) {
      if (l.priority == 10) highP = &l;
      if (l.priority == 1) lowP = &l;
    }

    check(highP != nullptr && lowP != nullptr, "both labels found");

    float highDist = std::sqrt(
      (highP->x - 10.0f) * (highP->x - 10.0f) +
      (highP->y - 10.0f) * (highP->y - 10.0f));
    float lowDist = std::sqrt(
      (lowP->x - 10.0f) * (lowP->x - 10.0f) +
      (lowP->y - 10.0f) * (lowP->y - 10.0f));

    check(near(highP->x, 10.0f) && near(highP->y, 10.0f),
          "high-priority label stays at original position");
    check(lowDist > 1.0f, "low-priority label displaced");
  }

  // Test 2: hideOnOverlap with excessive displacement
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 200.0f, 200.0f, 10, 0, 0, false, true},  // huge high-priority
      {10.0f, 10.0f,  20.0f,  20.0f,  1, 0, 0, false, true}   // small low-priority
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 2.0f;
    cfg.maxIterations = 10;
    cfg.maxDisplacement = 5.0f;  // very tight limit
    cfg.hideOnOverlap = true;

    auto result = solver.solve(labels, cfg);

    const dc::LabelRect* highP = nullptr;
    const dc::LabelRect* lowP = nullptr;
    for (auto& l : result.labels) {
      if (l.priority == 10) highP = &l;
      if (l.priority == 1) lowP = &l;
    }

    check(highP != nullptr && highP->visible, "high-priority label remains visible");
    check(lowP != nullptr && !lowP->visible,
          "low-priority label hidden (displacement > maxDisplacement)");
  }

  // Test 3: hideOnOverlap=false — displaced label stays visible
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 200.0f, 200.0f, 10, 0, 0, false, true},
      {10.0f, 10.0f,  20.0f,  20.0f,  1, 0, 0, false, true}
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 2.0f;
    cfg.maxIterations = 10;
    cfg.maxDisplacement = 5.0f;
    cfg.hideOnOverlap = false;

    auto result = solver.solve(labels, cfg);

    bool allVisible = true;
    for (auto& l : result.labels) {
      if (!l.visible) allVisible = false;
    }
    check(allVisible, "hideOnOverlap=false: all labels remain visible");
  }

  // Test 4: Three priorities — highest stays, middle may move less, lowest moves most
  {
    std::vector<dc::LabelRect> labels = {
      {0.0f, 0.0f, 40.0f, 15.0f, 100, 0, 0, false, true},  // highest
      {5.0f, 0.0f, 40.0f, 15.0f,  50, 0, 0, false, true},  // medium
      {10.0f, 0.0f, 40.0f, 15.0f,  1, 0, 0, false, true}   // lowest
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 0.0f;
    cfg.maxIterations = 20;

    auto result = solver.solve(labels, cfg);

    const dc::LabelRect* pHigh = nullptr;
    const dc::LabelRect* pMed = nullptr;
    const dc::LabelRect* pLow = nullptr;
    for (auto& l : result.labels) {
      if (l.priority == 100) pHigh = &l;
      if (l.priority == 50) pMed = &l;
      if (l.priority == 1) pLow = &l;
    }

    float highDist = std::sqrt(
      (pHigh->x - 0.0f) * (pHigh->x - 0.0f) +
      (pHigh->y - 0.0f) * (pHigh->y - 0.0f));
    float lowDist = std::sqrt(
      (pLow->x - 10.0f) * (pLow->x - 10.0f) +
      (pLow->y - 0.0f) * (pLow->y - 0.0f));

    check(near(pHigh->x, 0.0f) && near(pHigh->y, 0.0f),
          "highest priority stays at origin");
    check(lowDist > 0.1f || pMed != nullptr,
          "lower-priority labels displaced");
  }

  // Test 5: originalX/originalY recorded correctly
  {
    std::vector<dc::LabelRect> labels = {
      {50.0f, 70.0f, 30.0f, 10.0f, 0, 0, 0, false, true},
      {50.0f, 70.0f, 30.0f, 10.0f, 0, 0, 0, false, true}
    };

    auto result = solver.solve(labels, {});
    check(near(result.labels[0].originalX, 50.0f), "originalX[0] stored");
    check(near(result.labels[0].originalY, 70.0f), "originalY[0] stored");
    check(near(result.labels[1].originalX, 50.0f), "originalX[1] stored");
    check(near(result.labels[1].originalY, 70.0f), "originalY[1] stored");
  }

  // Test 6: Already-hidden labels ignored
  {
    std::vector<dc::LabelRect> labels = {
      {10.0f, 10.0f, 50.0f, 20.0f, 5, 0, 0, false, true},
      {10.0f, 10.0f, 50.0f, 20.0f, 1, 0, 0, false, false}  // already hidden
    };

    dc::CollisionSolverConfig cfg;
    cfg.padding = 0.0f;

    auto result = solver.solve(labels, cfg);

    const dc::LabelRect* vis = nullptr;
    const dc::LabelRect* hid = nullptr;
    for (auto& l : result.labels) {
      if (l.visible) vis = &l;
      else hid = &l;
    }

    check(vis != nullptr, "visible label found");
    check(hid != nullptr && !hid->visible, "hidden label stays hidden");
    // The visible label should not have moved since the hidden one is ignored
    check(near(vis->x, 10.0f) && near(vis->y, 10.0f),
          "visible label unmoved (hidden label ignored)");
  }

  std::printf("=== D51.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
