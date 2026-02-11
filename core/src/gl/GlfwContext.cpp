#ifdef DC_HAS_GLFW

#include "dc/gl/GlfwContext.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>

namespace dc {

GlfwContext::GlfwContext() = default;

GlfwContext::~GlfwContext() {
  if (window_) {
    glfwDestroyWindow(window_);
  }
  glfwTerminate();
}

bool GlfwContext::init(int width, int height) {
  if (!glfwInit()) {
    std::fprintf(stderr, "GlfwContext: glfwInit failed\n");
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  window_ = glfwCreateWindow(width, height, "DynaCharting", nullptr, nullptr);
  if (!window_) {
    std::fprintf(stderr, "GlfwContext: glfwCreateWindow failed\n");
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window_);

  int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
  if (!version) {
    std::fprintf(stderr, "GlfwContext: gladLoadGL failed\n");
    glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
    return false;
  }

  width_ = width;
  height_ = height;

  // Install callbacks
  glfwSetWindowUserPointer(window_, this);
  glfwSetScrollCallback(window_, scrollCallback);
  glfwSetCursorPosCallback(window_, cursorPosCallback);
  glfwSetMouseButtonCallback(window_, mouseButtonCallback);

  // Initialize cursor position
  glfwGetCursorPos(window_, &lastCursorX_, &lastCursorY_);

  return true;
}

void GlfwContext::swapBuffers() {
  if (window_) {
    glfwSwapBuffers(window_);
  }
}

std::vector<std::uint8_t> GlfwContext::readPixels() const {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width_) * height_ * 4);
  glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

bool GlfwContext::shouldClose() const {
  return window_ && glfwWindowShouldClose(window_);
}

InputState GlfwContext::pollInput() {
  glfwPollEvents();

  // Update framebuffer size
  if (window_) {
    glfwGetFramebufferSize(window_, &width_, &height_);
  }

  InputState state;
  state.shouldClose = shouldClose();
  state.cursorX = lastCursorX_;
  state.cursorY = lastCursorY_;
  state.zoomDelta = scrollAccum_;
  state.panDx = dragDx_;
  state.panDy = dragDy_;
  state.dragging = dragging_;

  // Reset accumulators
  scrollAccum_ = 0;
  dragDx_ = 0;
  dragDy_ = 0;

  return state;
}

void GlfwContext::scrollCallback(GLFWwindow* w, double /*xoff*/, double yoff) {
  auto* self = static_cast<GlfwContext*>(glfwGetWindowUserPointer(w));
  if (self) self->scrollAccum_ += yoff;
}

void GlfwContext::cursorPosCallback(GLFWwindow* w, double x, double y) {
  auto* self = static_cast<GlfwContext*>(glfwGetWindowUserPointer(w));
  if (!self) return;

  if (self->dragging_) {
    self->dragDx_ += x - self->lastCursorX_;
    self->dragDy_ += y - self->lastCursorY_;
  }
  self->lastCursorX_ = x;
  self->lastCursorY_ = y;
}

void GlfwContext::mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
  auto* self = static_cast<GlfwContext*>(glfwGetWindowUserPointer(w));
  if (!self) return;

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    self->dragging_ = (action == GLFW_PRESS);
  }
}

} // namespace dc

#endif // DC_HAS_GLFW
