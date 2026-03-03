#include "dc/interaction/ContextMenu.hpp"

namespace dc {

void ContextMenu::addProvider(ContextMenuProvider provider) {
  providers_.push_back(std::move(provider));
}

std::vector<ContextMenuItem> ContextMenu::buildMenu(const ContextMenuRequest& request) {
  std::vector<ContextMenuItem> result;
  for (auto& provider : providers_) {
    auto items = provider(request);
    result.insert(result.end(),
                  std::make_move_iterator(items.begin()),
                  std::make_move_iterator(items.end()));
  }
  return result;
}

void ContextMenu::show(const ContextMenuRequest& request) {
  currentItems_ = buildMenu(request);
  visible_ = true;
}

void ContextMenu::select(std::uint32_t itemId) {
  if (onSelect_) {
    onSelect_(itemId);
  }
  hide();
}

void ContextMenu::hide() {
  visible_ = false;
}

void ContextMenu::setOnSelect(ContextMenuSelectCallback cb) {
  onSelect_ = std::move(cb);
}

bool ContextMenu::isVisible() const {
  return visible_;
}

const std::vector<ContextMenuItem>& ContextMenu::currentItems() const {
  return currentItems_;
}

} // namespace dc
