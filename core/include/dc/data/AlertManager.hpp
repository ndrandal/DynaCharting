#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dc {

enum class AlertCondition : std::uint8_t {
  CrossingUp = 0,    // value crosses above threshold
  CrossingDown = 1,  // value crosses below threshold
  CrossingAny = 2,   // value crosses threshold in either direction
  GreaterThan = 3,   // value > threshold (fires once when condition becomes true)
  LessThan = 4,      // value < threshold
  EntersRange = 5,   // value enters [low, high]
  ExitsRange = 6     // value leaves [low, high]
};

struct Alert {
  std::uint32_t id{0};
  std::string name;
  AlertCondition condition{AlertCondition::CrossingUp};
  double threshold{0};        // for crossing/comparison
  double rangeLow{0}, rangeHigh{0};  // for range conditions
  bool triggered{false};      // has this alert fired?
  bool oneShot{true};         // if true, auto-disable after firing
  bool enabled{true};
  double lastValue{0};        // previous evaluation value (for crossing detection)
  bool initialized{false};    // has lastValue been set?
};

using AlertCallback = std::function<void(const Alert& alert, double currentValue)>;

class AlertManager {
public:
  // Create alerts
  std::uint32_t addCrossingAlert(const std::string& name, AlertCondition cond, double threshold);
  std::uint32_t addRangeAlert(const std::string& name, AlertCondition cond, double low, double high);

  // Configure
  void setOneShot(std::uint32_t id, bool oneShot);
  void setEnabled(std::uint32_t id, bool enabled);
  void remove(std::uint32_t id);
  void clear();
  void resetAll();  // reset triggered state on all alerts

  // Set the callback for when alerts fire
  void setCallback(AlertCallback cb);

  // Evaluate all alerts against a current value
  // Call this each time the watched value updates
  void evaluate(double currentValue);

  // Accessors
  const Alert* get(std::uint32_t id) const;
  const std::vector<Alert>& alerts() const;
  std::size_t count() const;
  std::size_t triggeredCount() const;

private:
  std::vector<Alert> alerts_;
  std::uint32_t nextId_{1};
  AlertCallback callback_;

  bool checkCondition(Alert& alert, double value);
};

} // namespace dc
