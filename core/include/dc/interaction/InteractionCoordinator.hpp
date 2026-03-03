#pragma once
#include "dc/ids/Id.hpp"
#include "dc/viewport/InputState.hpp"

namespace dc {

class InputMapper;
class DragDropState;
class BoxSelection;
class FocusManager;
class SelectionState;
class EventBus;
class Scene;
class IngestProcessor;
class Viewport;

struct InteractionConfig {
  bool enableBoxSelect{true};
  bool enableDragDrop{true};
  double dragThreshold{5.0};
};

struct InteractionResult {
  bool viewportChanged{false};
  bool selectionChanged{false};
  bool dragActive{false};
  bool boxSelectActive{false};
  bool focusChanged{false};
  Id clickedDrawItemId{0};
};

class InteractionCoordinator {
public:
  void setInputMapper(InputMapper* mapper) { inputMapper_ = mapper; }
  void setDragDropState(DragDropState* dds) { dragDrop_ = dds; }
  void setBoxSelection(BoxSelection* bs) { boxSelection_ = bs; }
  void setFocusManager(FocusManager* fm) { focusManager_ = fm; }
  void setSelectionState(SelectionState* ss) { selectionState_ = ss; }
  void setEventBus(EventBus* bus) { eventBus_ = bus; }

  void setConfig(const InteractionConfig& cfg) { config_ = cfg; }
  const InteractionConfig& config() const { return config_; }

  InteractionResult processFrame(const ViewportInputState& input,
                                 const Scene& scene,
                                 const IngestProcessor& ingest,
                                 Viewport* viewport,
                                 Id paneId);

private:
  InputMapper* inputMapper_{nullptr};
  DragDropState* dragDrop_{nullptr};
  BoxSelection* boxSelection_{nullptr};
  FocusManager* focusManager_{nullptr};
  SelectionState* selectionState_{nullptr};
  EventBus* eventBus_{nullptr};

  InteractionConfig config_;

  // Track drag state across frames
  bool wasDragging_{false};
};

} // namespace dc
