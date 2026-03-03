// D71.2 — HoverManager: tooltip provider, anchor modes, hover delay
#include "dc/interaction/HoverManager.hpp"

#include <cstdio>
#include <cmath>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D71.2 Tooltip Tests ===\n");

  // Test 1: Tooltip provider populates tooltip data
  {
    dc::HoverManager hm;
    hm.setTooltipProvider([](std::uint32_t drawItemId, double dataX, double dataY) -> dc::TooltipData {
      dc::TooltipData td;
      td.visible = true;
      td.drawItemId = drawItemId;
      td.dataX = dataX;
      td.dataY = dataY;
      td.title = "AAPL";
      td.fields.push_back({"Open", "150.00", {0, 1, 0, 1}});
      td.fields.push_back({"High", "155.00", {0, 1, 0, 1}});
      td.fields.push_back({"Low", "148.00", {1, 0, 0, 1}});
      td.fields.push_back({"Close", "153.50", {0, 1, 0, 1}});
      return td;
    });

    hm.update(100, 42.0, 153.5, 400.0, 300.0);
    const auto& tt = hm.tooltip();
    check(tt.visible, "provider: tooltip visible");
    check(tt.drawItemId == 100, "provider: drawItemId is 100");
    check(tt.title == "AAPL", "provider: title is AAPL");
    check(tt.fields.size() == 4, "provider: 4 fields");
    check(tt.fields[0].label == "Open", "provider: field[0] label is Open");
    check(tt.fields[0].value == "150.00", "provider: field[0] value is 150.00");
    check(tt.fields[2].label == "Low", "provider: field[2] label is Low");
    check(tt.fields[3].value == "153.50", "provider: field[3] value is 153.50");
  }

  // Test 2: Provider receives correct arguments
  {
    dc::HoverManager hm;
    std::uint32_t receivedId = 0;
    double receivedX = 0, receivedY = 0;
    hm.setTooltipProvider([&](std::uint32_t drawItemId, double dataX, double dataY) -> dc::TooltipData {
      receivedId = drawItemId;
      receivedX = dataX;
      receivedY = dataY;
      dc::TooltipData td;
      td.visible = true;
      return td;
    });

    hm.update(77, 12.5, 99.9, 200.0, 100.0);
    check(receivedId == 77, "provider args: drawItemId is 77");
    check(std::abs(receivedX - 12.5) < 1e-9, "provider args: dataX is 12.5");
    check(std::abs(receivedY - 99.9) < 1e-9, "provider args: dataY is 99.9");
  }

  // Test 3: Provider not called when drawItemId is 0
  {
    dc::HoverManager hm;
    bool providerCalled = false;
    hm.setTooltipProvider([&](std::uint32_t, double, double) -> dc::TooltipData {
      providerCalled = true;
      return {};
    });

    hm.update(0, 0, 0, 0, 0);
    check(!providerCalled, "provider not called for drawItemId 0");
  }

  // Test 4: Provider called again when target changes
  {
    dc::HoverManager hm;
    int callCount = 0;
    hm.setTooltipProvider([&](std::uint32_t, double, double) -> dc::TooltipData {
      ++callCount;
      dc::TooltipData td;
      td.visible = true;
      return td;
    });

    hm.update(1, 0, 0, 0, 0);
    hm.update(2, 0, 0, 0, 0);
    check(callCount == 2, "provider called twice for two different targets");
  }

  // Test 5: Provider NOT called for same target (cached tooltip)
  {
    dc::HoverManager hm;
    int callCount = 0;
    hm.setTooltipProvider([&](std::uint32_t, double, double) -> dc::TooltipData {
      ++callCount;
      dc::TooltipData td;
      td.visible = true;
      return td;
    });

    hm.update(5, 1.0, 2.0, 10.0, 20.0);
    hm.update(5, 3.0, 4.0, 30.0, 40.0);
    check(callCount == 1, "provider not re-called for same target");
  }

  // Test 6: Tooltip field color
  {
    dc::TooltipField field;
    field.label = "Volume";
    field.value = "1,234,567";
    field.color[0] = 0.2f;
    field.color[1] = 0.4f;
    field.color[2] = 0.6f;
    field.color[3] = 0.8f;
    check(std::abs(field.color[0] - 0.2f) < 1e-6f, "field color[0]");
    check(std::abs(field.color[1] - 0.4f) < 1e-6f, "field color[1]");
    check(std::abs(field.color[2] - 0.6f) < 1e-6f, "field color[2]");
    check(std::abs(field.color[3] - 0.8f) < 1e-6f, "field color[3]");
  }

  // Test 7: Default tooltip anchor is CursorFollow
  {
    dc::HoverManager hm;
    check(hm.tooltipAnchor() == dc::HoverManager::TooltipAnchor::CursorFollow,
          "default anchor: CursorFollow");
  }

  // Test 8: Set tooltip anchor to DataPoint
  {
    dc::HoverManager hm;
    hm.setTooltipAnchor(dc::HoverManager::TooltipAnchor::DataPoint);
    check(hm.tooltipAnchor() == dc::HoverManager::TooltipAnchor::DataPoint,
          "set anchor: DataPoint");
  }

  // Test 9: Set tooltip anchor to Fixed
  {
    dc::HoverManager hm;
    hm.setTooltipAnchor(dc::HoverManager::TooltipAnchor::Fixed);
    check(hm.tooltipAnchor() == dc::HoverManager::TooltipAnchor::Fixed,
          "set anchor: Fixed");
  }

  // Test 10: Default hover delay is 0
  {
    dc::HoverManager hm;
    check(hm.hoverDelay() == 0.0, "default hover delay is 0");
  }

  // Test 11: Set hover delay
  {
    dc::HoverManager hm;
    hm.setHoverDelay(250.0);
    check(std::abs(hm.hoverDelay() - 250.0) < 1e-9, "set hover delay to 250ms");
  }

  // Test 12: Set hover delay to custom value and verify
  {
    dc::HoverManager hm;
    hm.setHoverDelay(500.0);
    check(std::abs(hm.hoverDelay() - 500.0) < 1e-9, "set hover delay to 500ms");
    hm.setHoverDelay(0.0);
    check(hm.hoverDelay() == 0.0, "reset hover delay to 0");
  }

  // Test 13: Tooltip screen position updates when same target moves
  {
    dc::HoverManager hm;
    hm.update(10, 1.0, 2.0, 100.0, 200.0);
    check(hm.tooltip().screenX == 100.0, "screen pos: initial screenX");
    check(hm.tooltip().screenY == 200.0, "screen pos: initial screenY");

    hm.update(10, 3.0, 4.0, 300.0, 400.0);
    check(hm.tooltip().screenX == 300.0, "screen pos: updated screenX");
    check(hm.tooltip().screenY == 400.0, "screen pos: updated screenY");
  }

  // Test 14: Multiple fields with different colors from provider
  {
    dc::HoverManager hm;
    hm.setTooltipProvider([](std::uint32_t, double, double) -> dc::TooltipData {
      dc::TooltipData td;
      td.visible = true;
      td.title = "BTC/USD";
      td.fields.push_back({"Price", "42,150.50", {1, 1, 1, 1}});
      td.fields.push_back({"Change", "-2.3%", {1, 0, 0, 1}});
      return td;
    });

    hm.update(99, 0, 0, 0, 0);
    const auto& tt = hm.tooltip();
    check(tt.title == "BTC/USD", "multi-field: title");
    check(tt.fields.size() == 2, "multi-field: 2 fields");
    check(tt.fields[1].label == "Change", "multi-field: field[1] label");
    check(tt.fields[1].value == "-2.3%", "multi-field: field[1] value");
    check(std::abs(tt.fields[1].color[0] - 1.0f) < 1e-6f, "multi-field: field[1] red");
    check(std::abs(tt.fields[1].color[1] - 0.0f) < 1e-6f, "multi-field: field[1] green");
  }

  std::printf("=== D71.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
