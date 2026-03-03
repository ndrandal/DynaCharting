#include "dc/layout/CollisionSolver.hpp"

#include <algorithm>
#include <cmath>

namespace dc {

bool CollisionSolver::overlaps(const LabelRect& a, const LabelRect& b, float padding) const {
  return (a.x - padding) < (b.x + b.width + padding) &&
         (a.x + a.width + padding) > (b.x - padding) &&
         (a.y - padding) < (b.y + b.height + padding) &&
         (a.y + a.height + padding) > (b.y - padding);
}

void CollisionSolver::pushApart(LabelRect& mover, const LabelRect& fixed, float padding) {
  // Compute center of each label
  float moverCx = mover.x + mover.width * 0.5f;
  float moverCy = mover.y + mover.height * 0.5f;
  float fixedCx = fixed.x + fixed.width * 0.5f;
  float fixedCy = fixed.y + fixed.height * 0.5f;

  float dx = moverCx - fixedCx;
  float dy = moverCy - fixedCy;

  // Push in the axis of greatest center offset (or Y if exactly centered)
  if (std::fabs(dx) >= std::fabs(dy)) {
    // Push horizontally
    if (dx >= 0) {
      // Mover is to the right: push right
      float overlap = (fixed.x + fixed.width + padding) - (mover.x - padding);
      if (overlap > 0) mover.x += overlap;
    } else {
      // Mover is to the left: push left
      float overlap = (mover.x + mover.width + padding) - (fixed.x - padding);
      if (overlap > 0) mover.x -= overlap;
    }
  } else {
    // Push vertically
    if (dy >= 0) {
      // Mover is below (higher y): push down
      float overlap = (fixed.y + fixed.height + padding) - (mover.y - padding);
      if (overlap > 0) mover.y += overlap;
    } else {
      // Mover is above (lower y): push up
      float overlap = (mover.y + mover.height + padding) - (fixed.y - padding);
      if (overlap > 0) mover.y -= overlap;
    }
  }
}

CollisionResult CollisionSolver::solve(std::vector<LabelRect> labels,
                                        const CollisionSolverConfig& config) {
  // Store original positions
  for (auto& l : labels) {
    l.originalX = l.x;
    l.originalY = l.y;
  }

  // Sort by priority descending (high priority first — they stay in place)
  std::sort(labels.begin(), labels.end(),
    [](const LabelRect& a, const LabelRect& b) {
      return a.priority > b.priority;
    });

  int iter = 0;
  for (; iter < config.maxIterations; ++iter) {
    bool anyOverlap = false;

    for (std::size_t i = 0; i < labels.size(); ++i) {
      if (!labels[i].visible) continue;

      for (std::size_t j = i + 1; j < labels.size(); ++j) {
        if (!labels[j].visible) continue;

        if (overlaps(labels[i], labels[j], config.padding)) {
          anyOverlap = true;

          // Lower priority label moves (j has lower or equal priority due to sort)
          if (!labels[j].fixed) {
            pushApart(labels[j], labels[i], config.padding);
          } else if (!labels[i].fixed) {
            pushApart(labels[i], labels[j], config.padding);
          }
        }
      }
    }

    if (!anyOverlap) break;
  }

  // Check displacement limits and optionally hide
  if (config.hideOnOverlap) {
    for (auto& l : labels) {
      if (!l.visible) continue;
      float dx = l.x - l.originalX;
      float dy = l.y - l.originalY;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist > config.maxDisplacement) {
        l.visible = false;
      }
    }
  }

  return {std::move(labels), iter};
}

} // namespace dc
