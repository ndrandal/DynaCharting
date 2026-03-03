#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dc {

struct ContextMenuItem {
  std::uint32_t id{0};
  std::string label;
  bool enabled{true};
  bool checked{false};
  bool separator{false};
  std::vector<ContextMenuItem> children;
};

struct ContextMenuRequest {
  double pixelX{0}, pixelY{0};
  Id hitDrawItemId{0};
  Id hitPaneId{0};
};

using ContextMenuProvider = std::function<std::vector<ContextMenuItem>(const ContextMenuRequest&)>;
using ContextMenuSelectCallback = std::function<void(std::uint32_t itemId)>;

class ContextMenu {
public:
  void addProvider(ContextMenuProvider provider);
  std::vector<ContextMenuItem> buildMenu(const ContextMenuRequest& request);
  void show(const ContextMenuRequest& request);
  void select(std::uint32_t itemId);
  void hide();
  void setOnSelect(ContextMenuSelectCallback cb);
  bool isVisible() const;
  const std::vector<ContextMenuItem>& currentItems() const;

private:
  std::vector<ContextMenuProvider> providers_;
  ContextMenuSelectCallback onSelect_;
  std::vector<ContextMenuItem> currentItems_;
  bool visible_{false};
};

} // namespace dc
