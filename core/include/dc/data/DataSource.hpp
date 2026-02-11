#pragma once
#include <cstdint>
#include <vector>

namespace dc {

class DataSource {
public:
  virtual ~DataSource() = default;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool poll(std::vector<std::uint8_t>& batch) = 0;
  virtual bool isRunning() const = 0;
};

} // namespace dc
