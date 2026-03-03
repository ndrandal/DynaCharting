#include "dc/interaction/InteractionCoordinator.hpp"
#include "dc/viewport/InputMapper.hpp"
#include "dc/interaction/DragDropState.hpp"
#include "dc/selection/BoxSelection.hpp"
#include "dc/interaction/FocusManager.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"

namespace dc {

InteractionResult InteractionCoordinator::processFrame(
    const ViewportInputState& input,
    const Scene& scene,
    const IngestProcessor& ingest,
    Viewport* viewport,
    Id paneId)
{
  (void)scene;
  (void)ingest;
  (void)paneId;

  InteractionResult result;

  // 1. InputMapper: forward input for pan/zoom
  if (inputMapper_) {
    bool changed = inputMapper_->processInput(input);
    result.viewportChanged = changed;
  }

  // 2. Click handling: selection + events
  if (input.clicked) {
    // Placeholder: host can use renderPick separately to determine actual ID
    result.clickedDrawItemId = 0;

    if (selectionState_) {
      selectionState_->select({0, 0});
      result.selectionChanged = true;
    }

    if (eventBus_) {
      EventData geomEv;
      geomEv.type = EventType::GeometryClicked;
      geomEv.targetId = 0;
      if (viewport) {
        double dx = 0, dy = 0;
        viewport->pixelToData(input.cursorX, input.cursorY, dx, dy);
        geomEv.payload[0] = dx;
        geomEv.payload[1] = dy;
      }
      eventBus_->emit(geomEv);

      EventData selEv;
      selEv.type = EventType::SelectionChanged;
      selEv.targetId = 0;
      eventBus_->emit(selEv);
    }
  }

  // 3. Drag handling: DragDropState lifecycle
  if (dragDrop_ && config_.enableDragDrop) {
    if (input.dragging && !wasDragging_) {
      // Drag started this frame — begin detection
      double dataX = 0, dataY = 0;
      if (viewport) {
        viewport->pixelToData(input.cursorX, input.cursorY, dataX, dataY);
      }
      dragDrop_->beginDetect(0, 0, input.cursorX, input.cursorY, dataX, dataY);
    } else if (input.dragging && wasDragging_) {
      // Ongoing drag — update
      double dataX = 0, dataY = 0;
      if (viewport) {
        viewport->pixelToData(input.cursorX, input.cursorY, dataX, dataY);
      }
      dragDrop_->update(input.cursorX, input.cursorY, dataX, dataY);
    } else if (!input.dragging && wasDragging_) {
      // Drag ended — drop or cancel
      DragDropPhase phase = dragDrop_->phase();
      if (phase == DragDropPhase::Dragging) {
        double dataX = 0, dataY = 0;
        if (viewport) {
          viewport->pixelToData(input.cursorX, input.cursorY, dataX, dataY);
        }
        dragDrop_->drop(0, dataX, dataY);
      } else {
        dragDrop_->cancel();
      }
    }

    wasDragging_ = input.dragging;
    result.dragActive = (dragDrop_->phase() == DragDropPhase::Dragging ||
                         dragDrop_->phase() == DragDropPhase::Detecting);
  }

  // 4. Key handling: FocusManager
  if (focusManager_ && input.keyPressed != KeyCode::None) {
    bool handled = focusManager_->processKey(input.keyPressed);
    result.focusChanged = handled;
  }

  return result;
}

} // namespace dc
