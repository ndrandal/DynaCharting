#pragma once
#include <cstdint>
#include <vector>

namespace dc {

struct LabelRect {
  float x, y, width, height;
  int priority{0};
  float originalX{0}, originalY{0};
  bool fixed{false};
  bool visible{true};
};

struct CollisionSolverConfig {
  float padding{2.0f};
  int maxIterations{10};
  float maxDisplacement{100.0f};
  bool hideOnOverlap{false};
};

struct CollisionResult {
  std::vector<LabelRect> labels;
  int iterations{0};
};

class CollisionSolver {
public:
  CollisionResult solve(std::vector<LabelRect> labels,
                        const CollisionSolverConfig& config = {});

private:
  bool overlaps(const LabelRect& a, const LabelRect& b, float padding) const;
  void pushApart(LabelRect& mover, const LabelRect& fixed, float padding);
};

} // namespace dc
