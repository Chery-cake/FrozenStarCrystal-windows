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

  std::vector<vk::raii::CommandBuffer> pendingCmds_;

  void createImageViews();
  void releasePendingCommands();

public:
  SwapchainRenderTarget(
      const std::shared_ptr<graphics::vulkan::devices::WindowInfo> &win,
      const std::shared_ptr<graphics::vulkan::devices::Device> &dev);

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
