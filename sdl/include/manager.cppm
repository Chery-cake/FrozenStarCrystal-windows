module;

#include "FrozenStarCrystal-windows_export.h"
#include <SDL3/SDL.h>

export module windows.sdl:manager;

import std.compat;

import :structs;
import :window;

import concurrency;

export namespace windows::sdl {

inline constexpr concurrency::pool::Pool windowPoolDesc{.name = "windowPool"};

class FROZENSTARCRYSTAL_WINDOWS_API Manager {
private:
  std::shared_ptr<graphics::vulkan::instances::Instance> instance_;
  std::shared_ptr<graphics::vulkan::devices::Manager> deviceManager_;
  std::shared_ptr<WindowPool> windowPool_;

  std::vector<std::shared_ptr<Window>> windows_;
  std::mutex windowsMutex_;

  std::shared_ptr<graphics::vulkan::devices::Device> pickDevice();

public:
  Manager(
      const std::shared_ptr<graphics::vulkan::instances::Instance> &instance,
      const std::shared_ptr<graphics::vulkan::devices::Manager> &deviceManager,
      const std::shared_ptr<concurrency::pool::Manager> &poolMgr);
  Manager(
      const std::shared_ptr<graphics::vulkan::instances::Instance> &instance,
      const std::shared_ptr<graphics::vulkan::devices::Manager> &deviceManager,
      const std::shared_ptr<concurrency::pool::Manager> &poolMgr,
      SDL_InitFlags flags);
  ~Manager();

  Manager(const Manager &) = delete;
  Manager &operator=(const Manager &) = delete;
  Manager(Manager &&) = delete;
  Manager &operator=(Manager &&) = delete;

  // ── Window creation ───────────────────────────────────────
  std::shared_ptr<Window>
  createWindow(const std::string &title, uint32_t framesInFlight,
               graphics::vulkan::devices::Swapchain::SwapchainInfo swapInfo,
               SDL_WindowFlags flags);
  std::shared_ptr<Window> createWindow(const std::string &title, uint32_t width,
                                       uint32_t height);

  // ── Event loop (call on main thread) ──────────────────────
  void pollEvents();

  // ── Frame processing (async) ──────────────────────────────
  // Dispatches beginFrame() and endFrame() for every window in the thread
  // pool
  void beginFrame();
  void endFrame();

  void renderFrame();

  // Wait for all window tasks to complete.
  void waitIdle();
};

} // namespace windows::sdl
