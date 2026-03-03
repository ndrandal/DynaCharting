// D47.1 — PostProcessPass: brightness pass halves white pixels
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/PostProcessPass.hpp"

#include <cstdio>
#include <cstdint>
#include <vector>

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
  std::printf("=== D47.1 PostProcessPass Tests ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("SKIP: OSMesa not available\n");
    return 0;
  }

  // Test 1: Create a white source texture
  GLuint srcTex;
  glGenTextures(1, &srcTex);
  glBindTexture(GL_TEXTURE_2D, srcTex);
  std::vector<std::uint8_t> whitePixels(W * H * 4, 255);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  check(srcTex > 0, "source texture created");

  // Test 2: Create brightness pass (multiply by 0.5)
  static const char* kBrightnessFragSrc = R"GLSL(
    #version 330 core
    in vec2 v_uv;
    out vec4 outColor;
    uniform sampler2D u_texture;
    uniform float u_brightness;
    void main() {
      vec4 c = texture(u_texture, v_uv);
      outColor = vec4(c.rgb * u_brightness, c.a);
    }
  )GLSL";

  dc::PostProcessPass brightnessPass;
  bool initOk = brightnessPass.init(kBrightnessFragSrc);
  check(initOk, "brightness pass init succeeds");

  brightnessPass.setUniform("u_brightness", 0.5f);

  // Test 3: Apply the pass
  brightnessPass.apply(srcTex, W, H);
  GLuint outTex = brightnessPass.outputTexture();
  check(outTex > 0, "output texture created");

  // Test 4: Read back output and verify pixel values are halved
  GLuint readFbo;
  glGenFramebuffers(1, &readFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);

  std::vector<std::uint8_t> result(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &readFbo);

  // Center pixel should be approximately 127 (255 * 0.5)
  int cx = W / 2, cy = H / 2;
  int idx = (cy * W + cx) * 4;
  int r = result[idx], g = result[idx + 1], b = result[idx + 2];
  check(r > 110 && r < 145, "red channel ~127");
  check(g > 110 && g < 145, "green channel ~127");
  check(b > 110 && b < 145, "blue channel ~127");
  check(result[idx + 3] == 255, "alpha channel unchanged");

  // Test 5: PostProcessStack
  dc::PostProcessStack stack;
  bool stackOk = stack.init(W, H);
  check(stackOk, "stack init succeeds");
  check(stack.sceneFbo() > 0, "scene FBO created");
  check(stack.sceneTexture() > 0, "scene texture created");
  check(stack.passCount() == 0, "initially 0 passes");

  // Test 6: Add pass to stack
  stack.addPass("brightness", kBrightnessFragSrc);
  check(stack.passCount() == 1, "1 pass after add");

  // Test 7: Disable/enable pass
  stack.setPassEnabled("brightness", false);

  // Apply with disabled pass — output should be same as input
  GLuint stackOut = stack.apply(srcTex, W, H);
  check(stackOut == srcTex, "disabled pass returns input texture");

  stack.setPassEnabled("brightness", true);

  // Clean up
  glDeleteTextures(1, &srcTex);

  std::printf("=== D47.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
