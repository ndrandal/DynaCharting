#pragma once
#include "dc/gl/GlContext.hpp"
#include "dc/viewport/InputState.hpp"

#ifdef DC_HAS_GLFW

struct GLFWwindow;

namespace dc {

struct InputState {
  double cursorX{0};
  double cursorY{0};
  double panDx{0};
  double panDy{0};
  double zoomDelta{0};
  bool dragging{false};
  bool shouldClose{false};

  // Convert to generic ViewportInputState
  ViewportInputState toViewportInput() const {
    return {cursorX, cursorY, panDx, panDy, zoomDelta, dragging};
  }
};

class GlfwContext : public GlContext {
public:
  GlfwContext();
  ~GlfwContext() override;

  GlfwContext(const GlfwContext&) = delete;
  GlfwContext& operator=(const GlfwContext&) = delete;

  bool init(int width, int height) override;
  void swapBuffers() override;

  int width() const override { return width_; }
  int height() const override { return height_; }

  std::vector<std::uint8_t> readPixels() const override;

  InputState pollInput();
  bool shouldClose() const;

private:
  GLFWwindow* window_{nullptr};
  int width_{0};
  int height_{0};

  // Input accumulation (set via callbacks)
  double scrollAccum_{0};
  double lastCursorX_{0};
  double lastCursorY_{0};
  double dragDx_{0};
  double dragDy_{0};
  bool dragging_{false};

  static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
  static void cursorPosCallback(GLFWwindow* w, double x, double y);
  static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
};

} // namespace dc

#endif // DC_HAS_GLFW
