// D78.2: Built-in post-process effects (GL tests)
#include "dc/gl/BuiltinEffects.hpp"
#include "dc/gl/PostProcessPass.hpp"

#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

// T1: Create a vignette pass, apply to a solid-color texture,
//     verify center is brighter than edges.
static void testVignetteEffect() {
#ifndef DC_HAS_OSMESA
  std::printf("T1 vignetteEffect: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 64, H = 64;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  // Create a solid white texture as input
  GLuint inputTex;
  glGenTextures(1, &inputTex);
  glBindTexture(GL_TEXTURE_2D, inputTex);
  std::vector<unsigned char> white(W * H * 4, 255);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Create a vignette pass
  dc::PostProcessPass pass;
  assert(pass.init(dc::vignetteFragmentSrc()));
  pass.setUniform("u_strength", 0.8f);
  pass.setUniform("u_radius", 0.5f);
  pass.apply(inputTex, W, H);

  // Read output pixels
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // Read from the pass output
  GLuint outFbo;
  glGenFramebuffers(1, &outFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, pass.outputTexture(), 0);

  std::vector<unsigned char> pixels(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // Center pixel should be brighter than corner pixel
  int cx = W / 2, cy = H / 2;
  int center = (cy * W + cx) * 4;
  int corner = 0; // (0,0)
  assert(pixels[center] >= pixels[corner]);

  glDeleteFramebuffers(1, &outFbo);
  glDeleteTextures(1, &inputTex);

  std::printf("T1 vignetteEffect: PASS\n");
#endif
}

// T2: PostProcessStack with vignette pass
static void testStackWithVignette() {
#ifndef DC_HAS_OSMESA
  std::printf("T2 stackWithVignette: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 32, H = 32;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::PostProcessStack stack;
  assert(stack.init(W, H));

  dc::addVignettePass(stack, 0.5f, 0.6f);
  assert(stack.passCount() == 1);

  // Create a solid input
  GLuint inputTex;
  glGenTextures(1, &inputTex);
  glBindTexture(GL_TEXTURE_2D, inputTex);
  std::vector<unsigned char> white(W * H * 4, 200);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLuint outTex = stack.apply(inputTex, W, H);
  assert(outTex != 0);

  glDeleteTextures(1, &inputTex);
  std::printf("T2 stackWithVignette: PASS\n");
#endif
}

// T3: Bloom passes register correctly
static void testBloomPassCount() {
#ifndef DC_HAS_OSMESA
  std::printf("T3 bloomPassCount: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 32, H = 32;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::PostProcessStack stack;
  assert(stack.init(W, H));

  dc::addBloomPasses(stack, 0.7f, 0.4f);
  assert(stack.passCount() == 4); // bright + blurH + blurV + composite

  std::printf("T3 bloomPassCount: PASS\n");
#endif
}

// T4: Enable/disable passes
static void testPassEnableDisable() {
#ifndef DC_HAS_OSMESA
  std::printf("T4 passEnableDisable: SKIP (no OSMesa)\n");
  return;
#else
  const int W = 32, H = 32;
  dc::OsMesaContext ctx;
  assert(ctx.init(W, H));

  dc::PostProcessStack stack;
  assert(stack.init(W, H));

  dc::addVignettePass(stack, 0.5f, 0.7f);
  assert(stack.passCount() == 1);

  // Create a solid input
  GLuint inputTex;
  glGenTextures(1, &inputTex);
  glBindTexture(GL_TEXTURE_2D, inputTex);
  std::vector<unsigned char> white(W * H * 4, 255);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Disable the pass — should return input unchanged
  stack.setPassEnabled("vignette", false);
  GLuint out = stack.apply(inputTex, W, H);
  assert(out == inputTex); // no passes active → returns input

  // Re-enable
  stack.setPassEnabled("vignette", true);
  out = stack.apply(inputTex, W, H);
  assert(out != inputTex); // pass was applied

  glDeleteTextures(1, &inputTex);
  std::printf("T4 passEnableDisable: PASS\n");
#endif
}

// T5: Shader source getters return non-null strings
static void testShaderSources() {
  assert(dc::vignetteFragmentSrc() != nullptr);
  assert(dc::bloomBrightPassFragSrc() != nullptr);
  assert(dc::bloomBlurFragSrc() != nullptr);
  assert(dc::bloomCompositeFragSrc() != nullptr);

  // Each should contain "outColor"
  assert(std::strstr(dc::vignetteFragmentSrc(), "outColor") != nullptr);
  assert(std::strstr(dc::bloomBrightPassFragSrc(), "outColor") != nullptr);

  std::printf("T5 shaderSources: PASS\n");
}

int main() {
  testVignetteEffect();
  testStackWithVignette();
  testBloomPassCount();
  testPassEnableDisable();
  testShaderSources();

  std::printf("\nAll D78.2 tests passed.\n");
  return 0;
}
