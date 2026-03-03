#pragma once
#include "dc/style/Theme.hpp"
#include <functional>
#include <string>
#include <unordered_map>

namespace dc {

class CommandProcessor;

class ThemeManager {
public:
  ThemeManager(); // registers "Dark" and "Light" presets

  void registerTheme(const std::string& name, const Theme& theme);
  void setTheme(const std::string& name);
  void setTheme(const Theme& theme);
  const Theme& getTheme() const;
  const std::string& themeName() const;

  int applyTheme(CommandProcessor& cp, const ThemeTarget& target);

  static Theme interpolate(const Theme& t1, const Theme& t2, float t);

  void setOnThemeChanged(std::function<void(const Theme&)> cb);
  std::vector<std::string> registeredThemes() const;

private:
  std::unordered_map<std::string, Theme> presets_;
  Theme current_;
  std::string currentName_;
  std::function<void(const Theme&)> onChange_;
};

} // namespace dc
