#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <iostream>
#include <string>

static void apply(dc::CommandProcessor& cp, const std::string& json) {
  auto r = cp.applyJsonText(json);
  if (!r.ok) {
    std::cerr << "ERR applying: " << json
              << "\n  -> code=" << r.err.code
              << " msg=" << r.err.message
              << " details=" << r.err.details << "\n";
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

  apply(cp, R"({"cmd":"hello"})");
  dump(cp, "after hello (expect all empty)");

  apply(cp, R"({"cmd":"createPane","name":"Main"})");
  dump(cp, "after createPane (expect panes=[...])");

  apply(cp, R"({"cmd":"createLayer","paneId":1,"name":"L1"})");
  dump(cp, "after createLayer (expect layers=[...])");

  apply(cp, R"({"cmd":"createDrawItem","layerId":2,"name":"ItemA"})");
  apply(cp, R"({"cmd":"createDrawItem","layerId":2,"name":"ItemB"})");
  dump(cp, "after createDrawItems (expect drawItems size=2)");

  apply(cp, R"({"cmd":"beginFrame","frameId":1})");
  dump(cp, "in frame (expect inFrame=true)");
  apply(cp, R"({"cmd":"commitFrame","frameId":1})");
  dump(cp, "after commit (expect inFrame=false)");

  apply(cp, R"({"cmd":"delete","id":2})");
  dump(cp, "after delete layerId=2 (expect layers=[], drawItems=[])");

  apply(cp, R"({"cmd":"delete","id":1})");
  dump(cp, "after delete paneId=1 (expect all empty)");

  std::cout << "\nD1.1 smoke PASS\n";
  return 0;
}
