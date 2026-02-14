#pragma once
#include "dc/layout/PaneLayout.hpp"
#include "dc/ids/Id.hpp"

#include <cstddef>
#include <vector>

namespace dc {

class CommandProcessor;

struct LayoutPaneEntry {
  Id paneId;
  float fraction;
};

struct LayoutConfig {
  float gap{0.05f};
  float margin{0.05f};
  float minFraction{0.1f};
};

class LayoutManager {
public:
  void setConfig(const LayoutConfig& cfg);
  void setPanes(const std::vector<LayoutPaneEntry>& entries);
  void addPane(Id paneId, float fraction);
  void removePane(Id paneId);
  float getFraction(Id paneId) const;

  void resizeDivider(std::size_t dividerIndex, float delta);
  void applyLayout(CommandProcessor& cp);

  const std::vector<PaneRegion>& regions() const { return regions_; }
  std::size_t dividerCount() const;
  float dividerClipY(std::size_t dividerIndex) const;

  const std::vector<LayoutPaneEntry>& entries() const { return entries_; }

private:
  void recompute();

  LayoutConfig config_;
  std::vector<LayoutPaneEntry> entries_;
  std::vector<PaneRegion> regions_;
};

} // namespace dc
