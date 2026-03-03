// D48.1 — DrawItemAnimator color animation test
#include "dc/anim/DrawItemAnimator.hpp"
#include "dc/anim/AnimationManager.hpp"
#include "dc/scene/Scene.hpp"

#include <cmath>
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

static bool near(float a, float b, float eps = 0.05f) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D48.1 DrawItemAnimator Color Animation ===\n");

  // --- Test 1: Red to blue over 1.0s with Linear easing ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    // Create minimal scene: pane -> layer -> drawItem
    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 100; di.layerId = 10;
    di.color[0] = 1.0f; // R
    di.color[1] = 0.0f; // G
    di.color[2] = 0.0f; // B
    di.color[3] = 1.0f; // A
    scene.addDrawItem(di);

    // Animate from red (1,0,0,1) to blue (0,0,1,1) over 1s, Linear
    auto tweenId = animator.animateColor(100, 0.0f, 0.0f, 1.0f, 1.0f,
                                         1.0f, dc::EasingType::Linear);
    check(tweenId != 0, "animateColor returns valid ID");
    check(mgr.activeCount() == 1, "1 active tween");

    // Tick 0.5s -> halfway: expect purple (0.5, 0, 0.5, 1)
    mgr.tick(0.5f);
    const dc::DrawItem* result = scene.getDrawItem(100);
    check(result != nullptr, "drawItem still exists");
    check(near(result->color[0], 0.5f), "R ~0.5 at halfway");
    check(near(result->color[1], 0.0f), "G ~0.0 at halfway");
    check(near(result->color[2], 0.5f), "B ~0.5 at halfway");
    check(near(result->color[3], 1.0f), "A ~1.0 at halfway");

    // Tick another 0.5s -> complete: expect blue (0, 0, 1, 1)
    mgr.tick(0.5f);
    result = scene.getDrawItem(100);
    check(near(result->color[0], 0.0f), "R ~0.0 at end");
    check(near(result->color[1], 0.0f), "G ~0.0 at end");
    check(near(result->color[2], 1.0f), "B ~1.0 at end");
    check(near(result->color[3], 1.0f), "A ~1.0 at end");
    check(mgr.activeCount() == 0, "tween finished and removed");
  }

  // --- Test 2: Animate with alpha change ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 200; di.layerId = 10;
    di.color[0] = 1.0f; di.color[1] = 1.0f;
    di.color[2] = 1.0f; di.color[3] = 1.0f; // white, opaque
    scene.addDrawItem(di);

    // Animate to transparent black (0,0,0,0) over 1s
    animator.animateColor(200, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, dc::EasingType::Linear);

    mgr.tick(1.0f);
    const dc::DrawItem* result = scene.getDrawItem(200);
    check(near(result->color[0], 0.0f), "R->0 at end");
    check(near(result->color[3], 0.0f), "A->0 at end (fade out)");
  }

  // --- Test 3: animateOpacity only changes alpha ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 300; di.layerId = 10;
    di.color[0] = 0.5f; di.color[1] = 0.6f;
    di.color[2] = 0.7f; di.color[3] = 1.0f;
    scene.addDrawItem(di);

    animator.animateOpacity(300, 0.0f, 1.0f, dc::EasingType::Linear);

    mgr.tick(0.5f);
    const dc::DrawItem* result = scene.getDrawItem(300);
    check(near(result->color[0], 0.5f), "opacity: R unchanged");
    check(near(result->color[1], 0.6f), "opacity: G unchanged");
    check(near(result->color[2], 0.7f), "opacity: B unchanged");
    check(near(result->color[3], 0.5f), "opacity: A ~0.5 at halfway");

    mgr.tick(0.5f);
    result = scene.getDrawItem(300);
    check(near(result->color[3], 0.0f), "opacity: A ~0.0 at end");
  }

  // --- Test 4: animateLineWidth ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 400; di.layerId = 10;
    di.lineWidth = 1.0f;
    scene.addDrawItem(di);

    animator.animateLineWidth(400, 5.0f, 1.0f, dc::EasingType::Linear);

    mgr.tick(0.5f);
    const dc::DrawItem* result = scene.getDrawItem(400);
    check(near(result->lineWidth, 3.0f), "lineWidth ~3.0 at halfway");

    mgr.tick(0.5f);
    result = scene.getDrawItem(400);
    check(near(result->lineWidth, 5.0f), "lineWidth ~5.0 at end");
  }

  // --- Test 5: animatePointSize ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 500; di.layerId = 10;
    di.pointSize = 2.0f;
    scene.addDrawItem(di);

    animator.animatePointSize(500, 10.0f, 1.0f, dc::EasingType::Linear);

    mgr.tick(0.5f);
    const dc::DrawItem* result = scene.getDrawItem(500);
    check(near(result->pointSize, 6.0f), "pointSize ~6.0 at halfway");

    mgr.tick(0.5f);
    result = scene.getDrawItem(500);
    check(near(result->pointSize, 10.0f), "pointSize ~10.0 at end");
  }

  // --- Test 6: Invalid DrawItem ID returns 0 ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    auto id = animator.animateColor(9999, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    check(id == 0, "animateColor on invalid ID returns 0");
    check(mgr.activeCount() == 0, "no tween added for invalid ID");
  }

  // --- Test 7: Cancel mid-animation freezes value ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 600; di.layerId = 10;
    di.color[0] = 0.0f; di.color[1] = 0.0f;
    di.color[2] = 0.0f; di.color[3] = 1.0f;
    scene.addDrawItem(di);

    auto tweenId = animator.animateColor(600, 1.0f, 1.0f, 1.0f, 1.0f,
                                          2.0f, dc::EasingType::Linear);
    mgr.tick(1.0f); // halfway
    const dc::DrawItem* result = scene.getDrawItem(600);
    float frozenR = result->color[0];
    check(near(frozenR, 0.5f), "R ~0.5 before cancel");

    mgr.cancel(tweenId);
    mgr.tick(1.0f); // should not change anything
    result = scene.getDrawItem(600);
    check(near(result->color[0], frozenR), "R frozen after cancel");
  }

  // --- Test 8: EaseOutQuad easing (non-linear) ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 700; di.layerId = 10;
    di.color[0] = 0.0f; di.color[1] = 0.0f;
    di.color[2] = 0.0f; di.color[3] = 1.0f;
    scene.addDrawItem(di);

    // Default easing is EaseOutQuad
    animator.animateColor(700, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    mgr.tick(0.5f);
    const dc::DrawItem* result = scene.getDrawItem(700);
    // EaseOutQuad at t=0.5: 0.5*(2-0.5) = 0.75, so R should be ~0.75
    check(near(result->color[0], 0.75f), "EaseOutQuad: R ~0.75 at t=0.5");
  }

  std::printf("=== D48.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
