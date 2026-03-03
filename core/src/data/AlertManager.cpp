#include "dc/data/AlertManager.hpp"
#include <algorithm>

namespace dc {

std::uint32_t AlertManager::addCrossingAlert(const std::string& name, AlertCondition cond, double threshold) {
  Alert a;
  a.id = nextId_++;
  a.name = name;
  a.condition = cond;
  a.threshold = threshold;
  alerts_.push_back(std::move(a));
  return alerts_.back().id;
}

std::uint32_t AlertManager::addRangeAlert(const std::string& name, AlertCondition cond, double low, double high) {
  Alert a;
  a.id = nextId_++;
  a.name = name;
  a.condition = cond;
  a.rangeLow = low;
  a.rangeHigh = high;
  alerts_.push_back(std::move(a));
  return alerts_.back().id;
}

void AlertManager::setOneShot(std::uint32_t id, bool oneShot) {
  for (auto& a : alerts_) {
    if (a.id == id) { a.oneShot = oneShot; return; }
  }
}

void AlertManager::setEnabled(std::uint32_t id, bool enabled) {
  for (auto& a : alerts_) {
    if (a.id == id) { a.enabled = enabled; return; }
  }
}

void AlertManager::remove(std::uint32_t id) {
  alerts_.erase(
    std::remove_if(alerts_.begin(), alerts_.end(),
      [id](const Alert& a) { return a.id == id; }),
    alerts_.end());
}

void AlertManager::clear() {
  alerts_.clear();
}

void AlertManager::resetAll() {
  for (auto& a : alerts_) {
    a.triggered = false;
    a.enabled = true;
    a.initialized = false;
  }
}

void AlertManager::setCallback(AlertCallback cb) {
  callback_ = std::move(cb);
}

void AlertManager::evaluate(double currentValue) {
  for (auto& a : alerts_) {
    if (!a.enabled) continue;

    if (checkCondition(a, currentValue)) {
      a.triggered = true;
      if (callback_) {
        callback_(a, currentValue);
      }
      if (a.oneShot) {
        a.enabled = false;
      }
    }

    a.lastValue = currentValue;
    a.initialized = true;
  }
}

const Alert* AlertManager::get(std::uint32_t id) const {
  for (const auto& a : alerts_) {
    if (a.id == id) return &a;
  }
  return nullptr;
}

const std::vector<Alert>& AlertManager::alerts() const {
  return alerts_;
}

std::size_t AlertManager::count() const {
  return alerts_.size();
}

std::size_t AlertManager::triggeredCount() const {
  std::size_t n = 0;
  for (const auto& a : alerts_) {
    if (a.triggered) ++n;
  }
  return n;
}

bool AlertManager::checkCondition(Alert& alert, double value) {
  if (!alert.initialized) return false;

  double last = alert.lastValue;

  switch (alert.condition) {
    case AlertCondition::CrossingUp:
      return (last <= alert.threshold && value > alert.threshold);

    case AlertCondition::CrossingDown:
      return (last >= alert.threshold && value < alert.threshold);

    case AlertCondition::CrossingAny:
      return (last <= alert.threshold && value > alert.threshold) ||
             (last >= alert.threshold && value < alert.threshold);

    case AlertCondition::GreaterThan: {
      bool wasFalse = (last <= alert.threshold);
      bool isTrue = (value > alert.threshold);
      return wasFalse && isTrue;
    }

    case AlertCondition::LessThan: {
      bool wasFalse = (last >= alert.threshold);
      bool isTrue = (value < alert.threshold);
      return wasFalse && isTrue;
    }

    case AlertCondition::EntersRange: {
      bool wasOutside = (last < alert.rangeLow || last > alert.rangeHigh);
      bool isInside = (value >= alert.rangeLow && value <= alert.rangeHigh);
      return wasOutside && isInside;
    }

    case AlertCondition::ExitsRange: {
      bool wasInside = (last >= alert.rangeLow && last <= alert.rangeHigh);
      bool isOutside = (value < alert.rangeLow || value > alert.rangeHigh);
      return wasInside && isOutside;
    }
  }

  return false;
}

} // namespace dc
