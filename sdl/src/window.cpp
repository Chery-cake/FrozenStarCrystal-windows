module;

#include <SDL3/SDL.h>

module windows.sdl;

import std.compat;
import vulkan;

namespace windows::sdl {

namespace {
inline void decrement_and_notify(std::atomic<uint32_t> &counter) {
  if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    counter.notify_all();
  }
}
} // namespace

Window::Window(
    SDL_Window *sdlWin, const std::shared_ptr<WindowPool> &pool,
    const std::shared_ptr<graphics::vulkan::devices::WindowInfo> &windowInfo,
    const std::shared_ptr<graphics::vulkan::devices::Device> &device)
    : window_(sdlWin), windowInfo_(windowInfo), device_(device),
      windowPool_(pool) {
  compositor_ = std::make_unique<
      graphics::vulkan::compositors::Compositor<SwapchainRenderTarget>>(
      SwapchainRenderTarget{windowInfo_, device_}, device_);
}

Window::~Window() {
  waitIdle();
  compositor_.reset();
  windowInfo_.reset();
  device_.reset();
  SDL_DestroyWindow(window_);
}

void Window::handleEvent(const SDL_Event &e) {
  // Built‑in quit handling
  if (e.type == SDL_EVENT_QUIT) {
    shouldClose_.store(true);
  }

  // Hold a strong ref so the Window can't be destroyed while the callback runs.
  auto self = shared_from_this();
  pendingTasks_.fetch_add(1, std::memory_order_acq_rel);

  // Forward to all registered event callbacks.
  windowPool_->submit_detach(
      [self, e]() {
        {
          std::shared_lock lock(self->eventMtx_);
          std::ranges::for_each(self->eventCallbacks_,
                                [&e](auto &cp) { cp(e); });
        }
        decrement_and_notify(self->pendingTasks_);
      },
      Priority::Event);
}

void Window::executeFrame() {
  std::unique_lock lock(frameMtx_);
  if (compositor_) {
    compositor_->render().get();
  }
}

void Window::executeFrameAsync() {
  if (!frameMtx_.try_lock()) {
    return;
  }

  auto self = shared_from_this();
  pendingTasks_.fetch_add(1, std::memory_order_acq_rel);

  windowPool_->submit_detach(
      [self]() {
        std::unique_lock lock(self->frameMtx_, std::adopt_lock);
        if (self->compositor_) {
          self->compositor_->render().get(); // TODO change to use coroutine
        }
        decrement_and_notify(self->pendingTasks_);
      },
      Priority::Frame);
}

} // namespace windows::sdl
