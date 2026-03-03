#pragma once
#include "dc/ids/Id.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

struct Annotation {
  Id drawItemId{0};
  std::string role;
  std::string label;
  std::string value;
};

class AnnotationStore {
public:
  void set(Id drawItemId, const std::string& role,
           const std::string& label, const std::string& value);
  void remove(Id drawItemId);
  const Annotation* get(Id drawItemId) const;
  std::vector<Annotation> findByRole(const std::string& role) const;
  std::vector<Annotation> all() const;
  std::size_t count() const;
  void clear();

  std::string toJSON() const;
  void loadJSON(const std::string& json);

private:
  std::unordered_map<Id, Annotation> annotations_;
};

} // namespace dc
