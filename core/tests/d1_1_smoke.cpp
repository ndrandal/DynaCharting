#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <iostream>
#include <string>
#include <vector>

static void apply(dc::CommandProcessor& cp, const std::string& json) {
  auto r = cp.applyJsonText(json);
  if (!r.ok) {
    std::cerr << "ERR applying: " << json << "\n  -> " << r.error << "\n";
    std::exit(1);
  }
  if (r.createdId != 0) {
    std::cout << "createdId=" << r.createdId << " for cmd: " << json << "\n";
  }
}

static void dump(dc::CommandProcessor& cp, const char* label) {
  std::cout << "\n--- " << label << " ---\n";
  std::cout << cp.listResourcesJson() << "\n";
}

int main() {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // 1) hello + empty listing
  apply(cp, R"({"cmd":"hello"})");
  dump(cp, "after hello (expect all empty)");

  // 2) create a pane (auto id)
  apply(cp, R"({"cmd":"createPane","name":"Main"})");
  dump(cp, "after createPane (expect panes=[...])");

  // 3) create a layer under paneId=1 (NOTE: assumes first auto-id = 1)
  // If your registry ever changes allocation start, this is the only place to update.
  apply(cp, R"({"cmd":"createLayer","paneId":1,"name":"L1"})");
  dump(cp, "after createLayer (expect layers=[...])");

  // 4) create draw items under layerId=2 (assumes next auto-id)
  apply(cp, R"({"cmd":"createDrawItem","layerId":2,"name":"ItemA"})");
  apply(cp, R"({"cmd":"createDrawItem","layerId":2,"name":"ItemB"})");
  dump(cp, "after createDrawItems (expect drawItems size=2)");

  // 5) begin/commit frame just toggles state
  apply(cp, R"({"cmd":"beginFrame"})");
  dump(cp, "in frame (expect inFrame=true)");
  apply(cp, R"({"cmd":"commitFrame"})");
  dump(cp, "after commit (expect inFrame=false)");

  // 6) delete layer => must cascade delete its drawItems AND remove their IDs from registry
  apply(cp, R"({"cmd":"delete","id":2})");
  dump(cp, "after delete layerId=2 (expect layers=[], drawItems=[])");

  // 7) delete pane => should remove paneId=1 (layers already gone)
  apply(cp, R"({"cmd":"delete","id":1})");
  dump(cp, "after delete paneId=1 (expect all empty)");

  std::cout << "\nD1.1 smoke PASS\n";
  return 0;
}
