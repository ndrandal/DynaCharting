// D50.1 — ContextMenu: providers, conditional items, submenu, select callback, show/hide lifecycle
#include "dc/interaction/ContextMenu.hpp"

#include <cstdio>

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
  std::printf("=== D50.1 ContextMenu Tests ===\n");

  // ---- Test 1: Two providers, buildMenu concatenates items ----
  {
    dc::ContextMenu menu;

    menu.addProvider([](const dc::ContextMenuRequest&) {
      return std::vector<dc::ContextMenuItem>{
        {1, "Copy", true, false, false, {}},
        {2, "Paste", true, false, false, {}}
      };
    });

    menu.addProvider([](const dc::ContextMenuRequest&) {
      return std::vector<dc::ContextMenuItem>{
        {10, "Zoom In", true, false, false, {}},
        {11, "Zoom Out", true, false, false, {}}
      };
    });

    dc::ContextMenuRequest req;
    req.pixelX = 100;
    req.pixelY = 200;

    auto items = menu.buildMenu(req);
    check(items.size() == 4, "two providers produce 4 items");
    check(items[0].id == 1 && items[0].label == "Copy", "first item is Copy");
    check(items[1].id == 2 && items[1].label == "Paste", "second item is Paste");
    check(items[2].id == 10 && items[2].label == "Zoom In", "third item is Zoom In");
    check(items[3].id == 11 && items[3].label == "Zoom Out", "fourth item is Zoom Out");
  }

  // ---- Test 2: Conditional items based on hitDrawItemId ----
  {
    dc::ContextMenu menu;

    menu.addProvider([](const dc::ContextMenuRequest& req) {
      std::vector<dc::ContextMenuItem> items;
      items.push_back({1, "Select All", true, false, false, {}});
      if (req.hitDrawItemId != 0) {
        items.push_back({2, "Delete Selected", true, false, false, {}});
        items.push_back({3, "Properties", true, false, false, {}});
      }
      return items;
    });

    // No hit target
    dc::ContextMenuRequest reqEmpty;
    reqEmpty.hitDrawItemId = 0;
    auto items1 = menu.buildMenu(reqEmpty);
    check(items1.size() == 1, "no hit -> 1 item (Select All only)");
    check(items1[0].id == 1, "only Select All when no hit");

    // With hit target
    dc::ContextMenuRequest reqHit;
    reqHit.hitDrawItemId = 42;
    auto items2 = menu.buildMenu(reqHit);
    check(items2.size() == 3, "hit drawItem -> 3 items");
    check(items2[1].id == 2 && items2[1].label == "Delete Selected", "conditional Delete appears");
    check(items2[2].id == 3 && items2[2].label == "Properties", "conditional Properties appears");
  }

  // ---- Test 3: Submenu children ----
  {
    dc::ContextMenu menu;

    menu.addProvider([](const dc::ContextMenuRequest&) {
      dc::ContextMenuItem sub;
      sub.id = 100;
      sub.label = "Indicators";
      sub.children = {
        {101, "RSI", true, false, false, {}},
        {102, "MACD", true, false, false, {}},
        {103, "Bollinger", true, true, false, {}}
      };
      return std::vector<dc::ContextMenuItem>{sub};
    });

    dc::ContextMenuRequest req;
    auto items = menu.buildMenu(req);
    check(items.size() == 1, "submenu: 1 top-level item");
    check(items[0].label == "Indicators", "submenu label is Indicators");
    check(items[0].children.size() == 3, "submenu has 3 children");
    check(items[0].children[0].id == 101 && items[0].children[0].label == "RSI", "child 0 is RSI");
    check(items[0].children[1].id == 102 && items[0].children[1].label == "MACD", "child 1 is MACD");
    check(items[0].children[2].checked == true, "Bollinger is checked");
  }

  // ---- Test 4: select fires callback with correct id ----
  {
    dc::ContextMenu menu;

    std::uint32_t selectedId = 0;
    int callCount = 0;
    menu.setOnSelect([&](std::uint32_t id) {
      selectedId = id;
      ++callCount;
    });

    menu.select(42);
    check(callCount == 1, "select fires callback once");
    check(selectedId == 42, "select passes correct id");

    menu.select(99);
    check(callCount == 2, "second select fires callback again");
    check(selectedId == 99, "second select passes correct id");
  }

  // ---- Test 5: show/hide lifecycle ----
  {
    dc::ContextMenu menu;

    menu.addProvider([](const dc::ContextMenuRequest&) {
      return std::vector<dc::ContextMenuItem>{
        {1, "Item A", true, false, false, {}},
        {2, "Item B", false, false, false, {}}
      };
    });

    check(!menu.isVisible(), "initially not visible");
    check(menu.currentItems().empty(), "initially no current items");

    dc::ContextMenuRequest req;
    req.pixelX = 50;
    req.pixelY = 75;
    req.hitPaneId = 7;

    menu.show(req);
    check(menu.isVisible(), "visible after show");
    check(menu.currentItems().size() == 2, "currentItems populated after show");
    check(menu.currentItems()[0].label == "Item A", "currentItems[0] is Item A");
    check(menu.currentItems()[1].enabled == false, "currentItems[1] is disabled");

    menu.hide();
    check(!menu.isVisible(), "not visible after hide");

    // show again, then select (which auto-hides)
    menu.show(req);
    check(menu.isVisible(), "visible after second show");

    std::uint32_t cbId = 0;
    menu.setOnSelect([&](std::uint32_t id) { cbId = id; });
    menu.select(2);
    check(!menu.isVisible(), "select auto-hides");
    check(cbId == 2, "select callback received correct id");
  }

  // ---- Test 6: separator and enabled/disabled items ----
  {
    dc::ContextMenu menu;

    menu.addProvider([](const dc::ContextMenuRequest&) {
      return std::vector<dc::ContextMenuItem>{
        {1, "Cut", true, false, false, {}},
        {0, "", false, false, true, {}},  // separator
        {2, "Redo", false, false, false, {}}  // disabled
      };
    });

    dc::ContextMenuRequest req;
    auto items = menu.buildMenu(req);
    check(items.size() == 3, "separator test: 3 items");
    check(items[1].separator == true, "item[1] is separator");
    check(items[1].label.empty(), "separator has empty label");
    check(items[2].enabled == false, "Redo is disabled");
  }

  std::printf("=== D50.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
