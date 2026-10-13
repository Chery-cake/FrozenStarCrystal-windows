module;

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

module windows.sdl;

import std.compat;
import vulkan;

// ---- helper: pull a windowID out of an SDL_Event where meaningful ----
namespace {
SDL_WindowID eventWindowID(const SDL_Event &e) {
  switch (e.type) {
  // Window events
  case SDL_EVENT_WINDOW_SHOWN:
  case SDL_EVENT_WINDOW_HIDDEN:
  case SDL_EVENT_WINDOW_EXPOSED:
  case SDL_EVENT_WINDOW_MOVED:
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
  case SDL_EVENT_WINDOW_MINIMIZED:
  case SDL_EVENT_WINDOW_MAXIMIZED:
  case SDL_EVENT_WINDOW_RESTORED:
  case SDL_EVENT_WINDOW_MOUSE_ENTER:
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
  case SDL_EVENT_WINDOW_FOCUS_GAINED:
  case SDL_EVENT_WINDOW_FOCUS_LOST:
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
  case SDL_EVENT_WINDOW_HIT_TEST:
  case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
  case SDL_EVENT_WINDOW_OCCLUDED:
  case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
  case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
  case SDL_EVENT_WINDOW_DESTROYED:
    return e.window.windowID;

  // Keyboard
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
    return e.key.windowID;

  // Text
  case SDL_EVENT_TEXT_EDITING:
  case SDL_EVENT_TEXT_INPUT:
    return e.text.windowID;

  // Mouse
  case SDL_EVENT_MOUSE_MOTION:
    return e.motion.windowID;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    return e.button.windowID;
  case SDL_EVENT_MOUSE_WHEEL:
    return e.wheel.windowID;

  // Gamepad / joystick are device-scoped, not window-scoped:
  default:
    return 0;
  }
}
} // namespace

namespace windows::sdl {

Manager::Manager(
    const std::shared_ptr<graphics::vulkan::instances::Instance> &instance,
    const std::shared_ptr<graphics::vulkan::devices::Manager> &deviceManager,
    const std::shared_ptr<concurrency::pool::Manager> &poolMgr)
    : Manager(instance, deviceManager, poolMgr,
              SDL_INIT_VIDEO | SDL_INIT_EVENTS) {}

Manager::Manager(
    const std::shared_ptr<graphics::vulkan::instances::Instance> &instance,
    const std::shared_ptr<graphics::vulkan::devices::Manager> &deviceManager,
    const std::shared_ptr<concurrency::pool::Manager> &poolMgr,
    SDL_InitFlags flags)
    : instance_(instance), deviceManager_(deviceManager) {

  bool created =
      poolMgr->createPool<WindowPoolQueue, WindowTask>(&windowPoolDesc, 1);
  windowPool_ = poolMgr->getPool<WindowPoolQueue, WindowTask>(&windowPoolDesc);
  if (created) { // TODO decide a better amount of threads
    poolMgr->resizePool(&windowPoolDesc,
                        deviceManager_->getDeviceEntries().size());
  }
  if (SDL_Init(flags) == false) {
    throw std::runtime_error("SDL_Init failed: " + std::string(SDL_GetError()));
  }
}

Manager::~Manager() {
  if (windowPool_) {
    windowPool_->wait();
  }

  {
    std::lock_guard lock(windowsMutex_);
    windows_.clear();
  }

  windowPool_.reset();
  deviceManager_.reset();
  instance_.reset();
}

std::shared_ptr<graphics::vulkan::devices::Device> Manager::pickDevice() {
  auto entries = deviceManager_->getDeviceEntries();
  if (entries.empty()) {
    throw std::runtime_error("No GPU available");
  }

  // TODO create multiple ways to select the GPU
  // TODO: allow user override via config; score is already computed by Manager.
  auto best = std::ranges::max_element(
      entries, {}, &graphics::vulkan::devices::Manager::DeviceEntry::score);
  return best->device;
}

std::shared_ptr<Window> Manager::createWindow(
    const std::string &title, uint32_t framesInFlight,
    graphics::vulkan::devices::Swapchain::SwapchainInfo swapInfo,
    SDL_WindowFlags flags) {
  auto device = pickDevice();

  // Create SDL window with Vulkan flag
  SDL_Window *sdlWin =
      SDL_CreateWindow(title.c_str(), static_cast<int>(swapInfo.extent.width),
                       static_cast<int>(swapInfo.extent.height), flags);
  if (sdlWin == nullptr) {
    throw std::runtime_error(std::string("SDL_CreateWindow failed: ") +
                             SDL_GetError());
  }

  // Create Vulkan surface
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (SDL_Vulkan_CreateSurface(sdlWin, instance_->getInstance(), nullptr,
                               &surface) == false) {
    SDL_DestroyWindow(sdlWin);
    throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") +
                             SDL_GetError());
  }

  // Build WindowInfo and register with the graphics backend
  auto winInfo = std::make_shared<graphics::vulkan::devices::WindowInfo>();
  winInfo->surface = std::make_unique<vk::raii::SurfaceKHR>(
      instance_->getRaiiInstance(), surface);
  winInfo->instance = instance_->getInstancePtr();

  std::shared_ptr<Window> window;
  try {
    device->createWindow(winInfo, framesInFlight, swapInfo);
    // Register with graphics backend (creates swapchain etc.)
    window = std::make_shared<Window>(sdlWin, windowPool_, winInfo, device);
  } catch (...) {
    // Surface is owned by winInfo's unique_ptr, so it will be destroyed.
    // But we still need to tear down the SDL window.
    SDL_DestroyWindow(sdlWin);
    // If createWindow partially registered, remove it.
    device->removeWindow(winInfo);
    throw;
  }

  {
    std::lock_guard lock(windowsMutex_);
    windows_.push_back(window);
  }
  return window;
}

std::shared_ptr<Window> Manager::createWindow(const std::string &title,
                                              uint32_t width, uint32_t height) {
  graphics::vulkan::devices::Swapchain::SwapchainInfo swapInfo;
  swapInfo.extent = vk::Extent2D{width, height};
  return createWindow(title, 3, swapInfo,
                      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
}

void Manager::pollEvents() {
  // Snapshot once — windows_ won't change while we poll on the main thread,
  // but this also protects against a concurrent createWindow().
  std::vector<std::shared_ptr<Window>> windowsCopy;
  {
    std::lock_guard lock(windowsMutex_);
    windowsCopy = windows_;
  }

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    // Global events: broadcast.
    if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_TERMINATING) {
      std::ranges::for_each(windowsCopy,
                            [&e](auto &win) { win->handleEvent(e); });
      continue;
    }

    const SDL_WindowID id = eventWindowID(e);
    if (id == 0) {
      // Unrouted (joystick, gamepad, sensor, …) — broadcast.
      std::ranges::for_each(windowsCopy,
                            [&e](auto &win) { win->handleEvent(e); });
      continue;
    }

    auto it = std::ranges::find_if(windowsCopy, [id](const auto &win) {
      return SDL_GetWindowID(win->sdlWindow()) == id;
    });
    if (it != windowsCopy.end()) {
      (*it)->handleEvent(e);
    }
  }
}

void Manager::beginFrame() {
  std::vector<std::shared_ptr<Window>> windowsCopy;
  {
    std::lock_guard lock(windowsMutex_);
    windowsCopy = windows_;
  }
  // TODO improve with thread pool later, for asyncronous calls

  std::ranges::for_each(windowsCopy, [](std::shared_ptr<Window> &win) {
    if (!win->shouldClose()) {
      win->executeFrameAsync();
    }
  });
}

void Manager::endFrame() {
  // No explicit end needed because executeFrame handles submit and present.
  // TODO will be used for synchronization; arrange to wait for the coroutine
  if (windowPool_) {
    windowPool_->wait();
  }
}

void Manager::renderFrame() {
  std::vector<std::shared_ptr<Window>> windowsCopy;
  {
    std::lock_guard lock(windowsMutex_);
    windowsCopy = windows_;
  }
  // TODO improve with thread pool later, for asyncronous calls

  std::ranges::for_each(windowsCopy, [](std::shared_ptr<Window> &win) {
    if (!win->shouldClose()) {
      win->executeFrame();
    }
  });
}

void Manager::waitIdle() {
  if (windowPool_) {
    windowPool_->wait(); // wait for the pool to not have any tasks
  }
  std::vector<std::shared_ptr<Window>> windowsCopy;
  {
    std::lock_guard lock(windowsMutex_);
    windowsCopy = windows_;
  }
  std::ranges::for_each(windowsCopy,
                        [](std::shared_ptr<Window> &win) { win->waitIdle(); });
}

} // namespace windows::sdl
