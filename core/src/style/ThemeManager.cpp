#include "dc/style/ThemeManager.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <algorithm>

namespace dc {

// -------------------- helpers --------------------

static void lerpColor4(const float a[4], const float b[4], float t, float out[4]) {
  for (int i = 0; i < 4; ++i) {
    out[i] = a[i] + (b[i] - a[i]) * t;
  }
}

// -------------------- ThemeManager --------------------

ThemeManager::ThemeManager() {
  presets_["Dark"] = darkTheme();
  presets_["Light"] = lightTheme();
  presets_["Midnight"] = midnightTheme();
  presets_["Neon"] = neonTheme();
  presets_["Pastel"] = pastelTheme();
  presets_["Bloomberg"] = bloombergTheme();
  current_ = presets_["Dark"];
  currentName_ = "Dark";
}

void ThemeManager::registerTheme(const std::string& name, const Theme& theme) {
  presets_[name] = theme;
}

void ThemeManager::setTheme(const std::string& name) {
  auto it = presets_.find(name);
  if (it == presets_.end()) return;
  current_ = it->second;
  currentName_ = name;
  if (onChange_) onChange_(current_);
}

void ThemeManager::setTheme(const Theme& theme) {
  current_ = theme;
  currentName_ = theme.name;
  if (onChange_) onChange_(current_);
}

const Theme& ThemeManager::getTheme() const {
  return current_;
}

const std::string& ThemeManager::themeName() const {
  return currentName_;
}

int ThemeManager::applyTheme(CommandProcessor& cp, const ThemeTarget& target) {
  auto cmds = generateThemeCommands(current_, target);
  int count = 0;
  for (const auto& cmd : cmds) {
    auto res = cp.applyJsonText(cmd);
    if (res.ok) ++count;
  }
  return count;
}

Theme ThemeManager::interpolate(const Theme& t1, const Theme& t2, float t) {
  Theme out = t1; // start with t1 as base (copies name, etc.)

  lerpColor4(t1.backgroundColor, t2.backgroundColor, t, out.backgroundColor);
  lerpColor4(t1.candleUp, t2.candleUp, t, out.candleUp);
  lerpColor4(t1.candleDown, t2.candleDown, t, out.candleDown);
  lerpColor4(t1.gridColor, t2.gridColor, t, out.gridColor);
  lerpColor4(t1.tickColor, t2.tickColor, t, out.tickColor);
  lerpColor4(t1.labelColor, t2.labelColor, t, out.labelColor);

  out.gridLineWidth = t1.gridLineWidth + (t2.gridLineWidth - t1.gridLineWidth) * t;
  out.tickLineWidth = t1.tickLineWidth + (t2.tickLineWidth - t1.tickLineWidth) * t;

  // D78: grid dash/gap/opacity
  out.gridDashLength = t1.gridDashLength + (t2.gridDashLength - t1.gridDashLength) * t;
  out.gridGapLength = t1.gridGapLength + (t2.gridGapLength - t1.gridGapLength) * t;
  out.gridOpacity = t1.gridOpacity + (t2.gridOpacity - t1.gridOpacity) * t;

  lerpColor4(t1.crosshairColor, t2.crosshairColor, t, out.crosshairColor);

  for (int i = 0; i < Theme::kMaxOverlayColors; ++i) {
    lerpColor4(t1.overlayColors[i], t2.overlayColors[i], t, out.overlayColors[i]);
  }

  lerpColor4(t1.volumeUp, t2.volumeUp, t, out.volumeUp);
  lerpColor4(t1.volumeDown, t2.volumeDown, t, out.volumeDown);
  lerpColor4(t1.textColor, t2.textColor, t, out.textColor);
  lerpColor4(t1.highlightColor, t2.highlightColor, t, out.highlightColor);
  lerpColor4(t1.drawingColor, t2.drawingColor, t, out.drawingColor);

  // D78: pane border/separator
  lerpColor4(t1.paneBorderColor, t2.paneBorderColor, t, out.paneBorderColor);
  out.paneBorderWidth = t1.paneBorderWidth + (t2.paneBorderWidth - t1.paneBorderWidth) * t;
  lerpColor4(t1.separatorColor, t2.separatorColor, t, out.separatorColor);
  out.separatorWidth = t1.separatorWidth + (t2.separatorWidth - t1.separatorWidth) * t;

  return out;
}

void ThemeManager::setOnThemeChanged(std::function<void(const Theme&)> cb) {
  onChange_ = std::move(cb);
}

std::vector<std::string> ThemeManager::registeredThemes() const {
  std::vector<std::string> names;
  names.reserve(presets_.size());
  for (const auto& kv : presets_) {
    names.push_back(kv.first);
  }
  std::sort(names.begin(), names.end());
  return names;
}

} // namespace dc
