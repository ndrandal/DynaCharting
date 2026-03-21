// D80: BindingEvaluator — reactive data bindings between scene elements
#pragma once
#include "dc/document/SceneDocument.hpp"
#include "dc/data/DerivedBuffer.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/ids/Id.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class CommandProcessor;
class IngestProcessor;
class Scene;

class BindingEvaluator {
public:
  BindingEvaluator(CommandProcessor& cp, IngestProcessor& ingest,
                   DerivedBufferManager& derivedBufs);

  // Load all bindings from a document. Clears previous state.
  void loadBindings(const std::map<Id, DocBinding>& bindings);

  // Wire up EventBus subscriptions. Call after loadBindings.
  void attach(EventBus& bus, SelectionState& selection);
  void detach(EventBus& bus);

  // Evaluate bindings triggered by selection changes.
  // Returns output buffer IDs that were modified (caller syncs to GPU).
  std::vector<Id> onSelectionChanged(const SelectionState& selection);

  // Evaluate bindings triggered by hover changes.
  std::vector<Id> onHoverChanged(Id drawItemId, std::uint32_t recordIndex);

  // Evaluate bindings triggered by viewport range changes.
  std::vector<Id> onViewportChanged(const std::string& viewportName,
                                    double xMin, double xMax);

  // Evaluate bindings triggered by data value crossing a threshold.
  std::vector<Id> onDataChanged(const std::vector<Id>& touchedBufferIds);

  std::size_t bindingCount() const { return bindings_.size(); }

private:
  struct LiveBinding {
    Id id;
    DocBinding spec;
    SubscriptionId eventSubId{0};
    bool lastThresholdState{false};  // for threshold trigger edge detection
  };

  CommandProcessor& cp_;
  IngestProcessor& ingest_;
  DerivedBufferManager& derivedBufs_;

  std::vector<LiveBinding> bindings_;
  std::vector<SubscriptionId> subscriptions_;

  // Apply effects
  void applyFilterEffect(const LiveBinding& b,
                          const std::vector<std::uint32_t>& indices);
  void applyRangeEffect(const LiveBinding& b,
                         double xMin, double xMax);
  void applyVisibilityEffect(const DocBindingEffect& eff, bool active);
  void applyColorEffect(const DocBindingEffect& eff, bool active);

  void updateGeometryVertexCount(Id geometryId, Id outputBufferId,
                                  std::uint32_t recordStride);
};

} // namespace dc
