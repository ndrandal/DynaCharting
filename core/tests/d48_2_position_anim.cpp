// D48.2 — DrawItemAnimator transform/position animation test
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
  std::printf("=== D48.2 DrawItemAnimator Transform Animation ===\n");

  // --- Test 1: Animate tx from 0 to 1 over 1.0s ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    // Build scene: pane -> layer -> drawItem with transform
    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);

    dc::Transform tf;
    tf.id = 50;
    tf.params.tx = 0.0f;
    tf.params.ty = 0.0f;
    tf.params.sx = 1.0f;
    tf.params.sy = 1.0f;
    dc::recomputeMat3(tf);
    scene.addTransform(tf);

    dc::DrawItem di;
    di.id = 100; di.layerId = 10;
    di.transformId = 50;
    scene.addDrawItem(di);

    // Animate transform: tx 0->1, ty 0->0, sx 1->1, sy 1->1
    auto tweenId = animator.animateTransform(100, 1.0f, 0.0f, 1.0f, 1.0f,
                                              1.0f, dc::EasingType::Linear);
    check(tweenId != 0, "animateTransform returns valid ID");
    check(mgr.activeCount() == 1, "1 active tween");

    // Tick halfway
    mgr.tick(0.5f);
    const dc::Transform* result = scene.getTransform(50);
    check(result != nullptr, "transform exists");
    check(near(result->params.tx, 0.5f), "tx ~0.5 at halfway");
    check(near(result->params.ty, 0.0f), "ty ~0.0 at halfway");
    check(near(result->params.sx, 1.0f), "sx ~1.0 at halfway");
    check(near(result->params.sy, 1.0f), "sy ~1.0 at halfway");

    // Verify mat3 is recomputed: mat3[6] = tx
    check(near(result->mat3[6], 0.5f), "mat3[6] (tx) recomputed to ~0.5");
    // mat3[0] = sx = 1.0
    check(near(result->mat3[0], 1.0f), "mat3[0] (sx) = 1.0");

    // Tick to completion
    mgr.tick(0.5f);
    result = scene.getTransform(50);
    check(near(result->params.tx, 1.0f), "tx ~1.0 at end");
    check(near(result->mat3[6], 1.0f), "mat3[6] (tx) recomputed to ~1.0");
    check(mgr.activeCount() == 0, "tween finished and removed");
  }

  // --- Test 2: Animate all 4 params simultaneously ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);

    dc::Transform tf;
    tf.id = 51;
    tf.params = {0.0f, 0.0f, 1.0f, 1.0f};
    dc::recomputeMat3(tf);
    scene.addTransform(tf);

    dc::DrawItem di;
    di.id = 101; di.layerId = 10;
    di.transformId = 51;
    scene.addDrawItem(di);

    // Animate: tx 0->2, ty 0->3, sx 1->2, sy 1->4
    animator.animateTransform(101, 2.0f, 3.0f, 2.0f, 4.0f,
                              1.0f, dc::EasingType::Linear);

    mgr.tick(1.0f); // complete
    const dc::Transform* result = scene.getTransform(51);
    check(near(result->params.tx, 2.0f), "tx = 2.0 at end");
    check(near(result->params.ty, 3.0f), "ty = 3.0 at end");
    check(near(result->params.sx, 2.0f), "sx = 2.0 at end");
    check(near(result->params.sy, 4.0f), "sy = 4.0 at end");

    // Verify mat3: column-major [sx,0,0, 0,sy,0, tx,ty,1]
    check(near(result->mat3[0], 2.0f), "mat3[0] = sx = 2.0");
    check(near(result->mat3[4], 4.0f), "mat3[4] = sy = 4.0");
    check(near(result->mat3[6], 2.0f), "mat3[6] = tx = 2.0");
    check(near(result->mat3[7], 3.0f), "mat3[7] = ty = 3.0");
    check(near(result->mat3[8], 1.0f), "mat3[8] = 1.0");
  }

  // --- Test 3: Animate from non-zero start ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);

    dc::Transform tf;
    tf.id = 52;
    tf.params = {5.0f, 10.0f, 2.0f, 3.0f};
    dc::recomputeMat3(tf);
    scene.addTransform(tf);

    dc::DrawItem di;
    di.id = 102; di.layerId = 10;
    di.transformId = 52;
    scene.addDrawItem(di);

    // Animate from (5,10,2,3) -> (0,0,1,1) over 1s
    animator.animateTransform(102, 0.0f, 0.0f, 1.0f, 1.0f,
                              1.0f, dc::EasingType::Linear);

    mgr.tick(0.5f);
    const dc::Transform* result = scene.getTransform(52);
    check(near(result->params.tx, 2.5f), "tx: 5->0, at 0.5s = 2.5");
    check(near(result->params.ty, 5.0f), "ty: 10->0, at 0.5s = 5.0");
    check(near(result->params.sx, 1.5f), "sx: 2->1, at 0.5s = 1.5");
    check(near(result->params.sy, 2.0f), "sy: 3->1, at 0.5s = 2.0");

    mgr.tick(0.5f);
    result = scene.getTransform(52);
    check(near(result->params.tx, 0.0f), "tx = 0.0 at end");
    check(near(result->params.ty, 0.0f), "ty = 0.0 at end");
    check(near(result->params.sx, 1.0f), "sx = 1.0 at end");
    check(near(result->params.sy, 1.0f), "sy = 1.0 at end");
  }

  // --- Test 4: No transform (transformId == 0) returns 0 ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);
    dc::DrawItem di;
    di.id = 103; di.layerId = 10;
    di.transformId = 0; // no transform
    scene.addDrawItem(di);

    auto id = animator.animateTransform(103, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    check(id == 0, "animateTransform returns 0 when no transform bound");
    check(mgr.activeCount() == 0, "no tween added");
  }

  // --- Test 5: Invalid drawItem ID returns 0 ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    auto id = animator.animateTransform(9999, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    check(id == 0, "animateTransform on invalid ID returns 0");
  }

  // --- Test 6: Multiple simultaneous animations ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);

    dc::Transform tf;
    tf.id = 53;
    tf.params = {0.0f, 0.0f, 1.0f, 1.0f};
    dc::recomputeMat3(tf);
    scene.addTransform(tf);

    dc::DrawItem di;
    di.id = 104; di.layerId = 10;
    di.transformId = 53;
    di.color[0] = 0.0f; di.color[1] = 0.0f;
    di.color[2] = 0.0f; di.color[3] = 1.0f;
    di.lineWidth = 1.0f;
    scene.addDrawItem(di);

    // Animate color + transform + lineWidth simultaneously
    animator.animateColor(104, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, dc::EasingType::Linear);
    animator.animateTransform(104, 10.0f, 0.0f, 1.0f, 1.0f, 1.0f, dc::EasingType::Linear);
    animator.animateLineWidth(104, 5.0f, 1.0f, dc::EasingType::Linear);
    check(mgr.activeCount() == 3, "3 simultaneous tweens");

    mgr.tick(1.0f);
    const dc::DrawItem* resultDI = scene.getDrawItem(104);
    const dc::Transform* resultTF = scene.getTransform(53);
    check(near(resultDI->color[0], 1.0f), "color animated to white");
    check(near(resultTF->params.tx, 10.0f), "tx animated to 10");
    check(near(resultDI->lineWidth, 5.0f), "lineWidth animated to 5");
    check(mgr.activeCount() == 0, "all tweens finished");
  }

  // --- Test 7: EaseOutQuad easing on transform ---
  {
    dc::Scene scene;
    dc::AnimationManager mgr;
    dc::DrawItemAnimator animator(mgr, scene);

    dc::Pane pane; pane.id = 1;
    scene.addPane(pane);
    dc::Layer layer; layer.id = 10; layer.paneId = 1;
    scene.addLayer(layer);

    dc::Transform tf;
    tf.id = 54;
    tf.params = {0.0f, 0.0f, 1.0f, 1.0f};
    dc::recomputeMat3(tf);
    scene.addTransform(tf);

    dc::DrawItem di;
    di.id = 105; di.layerId = 10;
    di.transformId = 54;
    scene.addDrawItem(di);

    // EaseOutQuad: at t=0.5, eased = 0.5*(2-0.5) = 0.75
    animator.animateTransform(105, 100.0f, 0.0f, 1.0f, 1.0f,
                              1.0f, dc::EasingType::EaseOutQuad);

    mgr.tick(0.5f);
    const dc::Transform* result = scene.getTransform(54);
    check(near(result->params.tx, 75.0f), "EaseOutQuad: tx ~75 at t=0.5");
  }

  std::printf("=== D48.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
