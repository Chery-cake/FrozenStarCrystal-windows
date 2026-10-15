module;

#include "FrozenStarCrystal-windows_export.h"

export module windows.sdl:targets;

import std.compat;

import graphics;

export namespace windows::sdl {

// Swapchain-based render target for the compositor
class FROZENSTARCRYSTAL_WINDOWS_API SwapchainRenderTarget {
private:
  std::shared_ptr<graphics::vulkan::devices::WindowInfo> windowInfo_;
  std::shared_ptr<graphics::vulkan::devices::Device> device_;
  std::vector<std::unique_ptr<vk::raii::ImageView>> imageViews_;
  vk::Image currentImage_ = nullptr;
  uint32_t currentIndex_ = 0;
  std::array<float, 4> defaultClearColor_ = {0.0f, 0.0f, 0.0f, 1.0f};

  uint32_t queueFamily_{0};
  std::unique_ptr<graphics::vulkan::devices::CommandBufferPool> cmdPool_;

  std::vector<std::optional<vk::raii::CommandBuffer>> pendingCmds_;
  // Which slot beginFrame() armed for the current frame.
  uint32_t activeSlot_ = 0;

  void createImageViews();
  void ensureCommandPool();

  void releaseSlotCommand(uint32_t slot);
  void releaseAllCommands();

public:
  SwapchainRenderTarget(
      const std::shared_ptr<graphics::vulkan::devices::WindowInfo> &win,
      const std::shared_ptr<graphics::vulkan::devices::Device> &dev);
  ~SwapchainRenderTarget();

  SwapchainRenderTarget(const SwapchainRenderTarget &) = delete;
  SwapchainRenderTarget &operator=(const SwapchainRenderTarget &) = delete;
  SwapchainRenderTarget(SwapchainRenderTarget &&) noexcept = default;
  SwapchainRenderTarget &operator=(SwapchainRenderTarget &&) noexcept = default;

  graphics::vulkan::compositors::FrameContext beginFrame();
  void endFrame(graphics::vulkan::compositors::FrameContext &fc);

  vk::Extent2D extent() const {
    return windowInfo_->swapchain->getinfo().extent;
  }
  vk::Format colorFormat() const {
    return windowInfo_->swapchain->getinfo().surfaceFormat.format;
  }
  void recreate();
};

} // namespace windows::sdl
