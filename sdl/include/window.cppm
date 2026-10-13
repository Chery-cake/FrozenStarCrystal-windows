module;

#include "FrozenStarCrystal-windows_export.h"
#include <SDL3/SDL.h>

export module windows.sdl:window;

import std.compat;

import graphics;
import concurrency;

import :structs;
import :targets;

export namespace windows::sdl {

class Manager;

class FROZENSTARCRYSTAL_WINDOWS_API Window
    : public std::enable_shared_from_this<Window> {
public:
  using EventCallback = std::move_only_function<void(const SDL_Event &)>;

  using SceneId = graphics::vulkan::compositors::SceneId;
  using ScenePosition = graphics::vulkan::compositors::Compositor<
      SwapchainRenderTarget>::Position;

private:
  SDL_Window *window_ = nullptr;
  std::atomic<bool> shouldClose_ = false;

  std::vector<EventCallback> eventCallbacks_;
  std::shared_mutex eventMtx_;

  std::shared_ptr<graphics::vulkan::devices::WindowInfo> windowInfo_;
  std::shared_ptr<graphics::vulkan::devices::Device> device_;

  std::unique_ptr<
      graphics::vulkan::compositors::Compositor<SwapchainRenderTarget>>
      compositor_;

  std::shared_ptr<WindowPool> windowPool_;
  std::shared_mutex frameMtx_;

  std::atomic<uint32_t> pendingTasks_{0};

public:
  Window(
      SDL_Window *sdlWin, const std::shared_ptr<WindowPool> &pool,
      const std::shared_ptr<graphics::vulkan::devices::WindowInfo> &windowInfo,
      const std::shared_ptr<graphics::vulkan::devices::Device> &device);
  ~Window();

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
  Window(Window &&) = delete;
  Window &operator=(Window &&) = delete;

  // Callback registration
  void onEvent(EventCallback cb) {
    std::unique_lock lock(eventMtx_);
    eventCallbacks_.push_back(std::move(cb));
  }

  // Called by Manager
  void handleEvent(const SDL_Event &e);
  void executeFrame();
  void executeFrameAsync();

  // Scene management (through compositor)
  template <graphics::vulkan::compositors::Scene S>
  SceneId addScene(S &&scene) {
    return compositor_->addScene(std::forward<S>(scene));
  }

  template <graphics::vulkan::compositors::Scene S>
  SceneId addScene(S &&scene, ScenePosition pos) {
    return compositor_->addScene(std::forward<S>(scene), pos);
  }

  bool removeScene(const SceneId &id) { return compositor_->removeScene(id); }
  bool removeScenes(std::span<const SceneId> ids) {
    return compositor_->removeScenes(ids);
  }
  void clearScenes() { compositor_->clearSceneFactories(); }

  // Access the compositor (for advanced use)
  auto &compositor() { return *compositor_; }
  [[nodiscard]] const auto &compositor() const { return *compositor_; }

  // Accessors
  [[nodiscard]] SDL_Window *sdlWindow() const { return window_; }
  [[nodiscard]] bool shouldClose() const { return shouldClose_.load(); }

  [[nodiscard]] std::pair<int, int> windowSize() const {
    int w;
    int h;
    SDL_GetWindowSize(window_, &w, &h);
    return {w, h};
  }
  [[nodiscard]] std::pair<int, int> windowSizeInPixels() const {
    int w;
    int h;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    return {w, h};
  }

  void waitIdle() {
    while (pendingTasks_.load(std::memory_order_acquire) != 0) {
      std::this_thread::yield();
    }
  }
};

} // namespace windows::sdl
