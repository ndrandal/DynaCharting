#include "dc/layout/LayoutManager.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <algorithm>
#include <string>

namespace dc {

void LayoutManager::setConfig(const LayoutConfig& cfg) {
  config_ = cfg;
}

void LayoutManager::setPanes(const std::vector<LayoutPaneEntry>& entries) {
  entries_ = entries;
  recompute();
}

void LayoutManager::addPane(Id paneId, float fraction) {
  entries_.push_back({paneId, fraction});
  recompute();
}

void LayoutManager::removePane(Id paneId) {
  entries_.erase(
    std::remove_if(entries_.begin(), entries_.end(),
                   [paneId](const LayoutPaneEntry& e) { return e.paneId == paneId; }),
    entries_.end());
  recompute();
}

float LayoutManager::getFraction(Id paneId) const {
  for (const auto& e : entries_) {
    if (e.paneId == paneId) return e.fraction;
  }
  return 0.0f;
}

void LayoutManager::resizeDivider(std::size_t dividerIndex, float delta) {
  if (dividerIndex >= dividerCount()) return;

  auto& above = entries_[dividerIndex];
  auto& below = entries_[dividerIndex + 1];

  float totalFrac = 0.0f;
  for (const auto& e : entries_) totalFrac += e.fraction;

  // Clamp delta so neither pane goes below minFraction
  float maxGrow = below.fraction - config_.minFraction * totalFrac;
  float maxShrink = above.fraction - config_.minFraction * totalFrac;

  float clampedDelta = std::max(-maxShrink, std::min(delta, maxGrow));

  above.fraction += clampedDelta;
  below.fraction -= clampedDelta;

  recompute();
}

void LayoutManager::recompute() {
  std::vector<float> fractions;
  fractions.reserve(entries_.size());
  for (const auto& e : entries_) {
    fractions.push_back(e.fraction);
  }
  regions_ = computePaneLayout(fractions, config_.gap, config_.margin);
}

void LayoutManager::applyLayout(CommandProcessor& cp) {
  recompute();
  for (std::size_t i = 0; i < entries_.size() && i < regions_.size(); i++) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setPaneRegion","id":%llu,"clipYMin":%.9g,"clipYMax":%.9g,"clipXMin":%.9g,"clipXMax":%.9g})",
      static_cast<unsigned long long>(entries_[i].paneId),
      static_cast<double>(regions_[i].clipYMin),
      static_cast<double>(regions_[i].clipYMax),
      static_cast<double>(regions_[i].clipXMin),
      static_cast<double>(regions_[i].clipXMax));
    cp.applyJsonText(buf);
  }
}

std::size_t LayoutManager::dividerCount() const {
  return entries_.size() > 1 ? entries_.size() - 1 : 0;
}

float LayoutManager::dividerClipY(std::size_t dividerIndex) const {
  if (dividerIndex >= dividerCount() || regions_.size() <= dividerIndex + 1) return 0.0f;
  // Midpoint of gap between pane[i].clipYMin and pane[i+1].clipYMax
  return (regions_[dividerIndex].clipYMin + regions_[dividerIndex + 1].clipYMax) / 2.0f;
}

} // namespace dc
