// D47.2 — PostProcessPass: blur pass smooths a sharp edge
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/PostProcessPass.hpp"

#include <cstdio>
#include <cstdint>
#include <cmath>
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
  std::printf("=== D47.2 Blur Pass Tests ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("SKIP: OSMesa not available\n");
    return 0;
  }

  // Create source texture with a sharp vertical edge:
  // Left half = white (255), right half = black (0)
  std::vector<std::uint8_t> edgePixels(W * H * 4);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int idx = (y * W + x) * 4;
      bool isLeft = (x < W / 2);
      std::uint8_t v = isLeft ? 255 : 0;
      edgePixels[idx + 0] = v;
      edgePixels[idx + 1] = v;
      edgePixels[idx + 2] = v;
      edgePixels[idx + 3] = 255;
    }
  }

  GLuint srcTex;
  glGenTextures(1, &srcTex);
  glBindTexture(GL_TEXTURE_2D, srcTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, edgePixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Test 1: Read back source to verify sharp edge
  {
    GLuint readFbo;
    glGenFramebuffers(1, &readFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);
    std::vector<std::uint8_t> srcRead(W * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, srcRead.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &readFbo);

    int cy = H / 2;
    int leftIdx = (cy * W + W / 4) * 4;
    int rightIdx = (cy * W + 3 * W / 4) * 4;
    check(srcRead[leftIdx] > 240, "source: left side is white");
    check(srcRead[rightIdx] < 15, "source: right side is black");
  }

  // Test 2: Apply horizontal blur pass
  static const char* kBlurFragSrc = R"GLSL(
    #version 330 core
    in vec2 v_uv;
    out vec4 outColor;
    uniform sampler2D u_texture;
    uniform vec2 u_resolution;
    void main() {
      vec2 texelSize = 1.0 / u_resolution;
      vec4 sum = vec4(0.0);
      // 5-tap horizontal blur
      sum += texture(u_texture, v_uv + vec2(-2.0 * texelSize.x, 0.0)) * 0.06;
      sum += texture(u_texture, v_uv + vec2(-1.0 * texelSize.x, 0.0)) * 0.24;
      sum += texture(u_texture, v_uv) * 0.40;
      sum += texture(u_texture, v_uv + vec2( 1.0 * texelSize.x, 0.0)) * 0.24;
      sum += texture(u_texture, v_uv + vec2( 2.0 * texelSize.x, 0.0)) * 0.06;
      outColor = sum;
    }
  )GLSL";

  dc::PostProcessPass blurPass;
  bool initOk = blurPass.init(kBlurFragSrc);
  check(initOk, "blur pass init succeeds");

  blurPass.apply(srcTex, W, H);
  GLuint outTex = blurPass.outputTexture();
  check(outTex > 0, "blur output texture created");

  // Test 3: Read back blurred output
  GLuint readFbo;
  glGenFramebuffers(1, &readFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
  std::vector<std::uint8_t> blurred(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, blurred.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &readFbo);

  // At the edge (x = W/2), the blur should create intermediate values
  int cy = H / 2;
  int edgeX = W / 2;
  int edgeIdx = (cy * W + edgeX) * 4;
  int edgeVal = blurred[edgeIdx];

  // The pixel right at the edge should be somewhere between 0 and 255
  // (blur smooths the sharp transition)
  check(edgeVal > 10 && edgeVal < 245, "edge pixel is intermediate value after blur");

  // Far left should still be very white (barely affected by blur)
  int farLeftIdx = (cy * W + 5) * 4;
  check(blurred[farLeftIdx] > 220, "far left still bright after blur");

  // Far right should still be very dark
  int farRightIdx = (cy * W + W - 6) * 4;
  check(blurred[farRightIdx] < 35, "far right still dark after blur");

  // Test 4: Stack with multiple passes (double blur)
  dc::PostProcessStack stack;
  bool stackOk = stack.init(W, H);
  check(stackOk, "stack init for multi-pass");

  stack.addPass("blur1", kBlurFragSrc);
  stack.addPass("blur2", kBlurFragSrc);
  check(stack.passCount() == 2, "stack has 2 passes");

  GLuint doubleBlurred = stack.apply(srcTex, W, H);
  check(doubleBlurred > 0, "double-blur output texture valid");

  // Read back double-blurred result
  glGenFramebuffers(1, &readFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, doubleBlurred, 0);
  std::vector<std::uint8_t> dblResult(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, dblResult.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &readFbo);

  // Double-blurred edge should be even more gradual
  int dblEdgeVal = dblResult[edgeIdx];
  check(dblEdgeVal > 5 && dblEdgeVal < 250, "double-blur edge pixel is intermediate");

  // Clean up
  glDeleteTextures(1, &srcTex);

  std::printf("=== D47.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
