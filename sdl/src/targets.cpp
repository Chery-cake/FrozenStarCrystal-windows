module;

module windows.sdl;

import std.compat;

namespace windows::sdl {

SwapchainRenderTarget::SwapchainRenderTarget(
    const std::shared_ptr<graphics::vulkan::devices::WindowInfo> &win,
    const std::shared_ptr<graphics::vulkan::devices::Device> &dev)
    : windowInfo_(win), device_(dev) {
  if (!windowInfo_ || !windowInfo_->swapchain) {
    throw std::logic_error(
        "[SwapchainRenderTarget] WindowInfo/swapchain not initialized");
  }

  const auto &qfi = device_->getQueueFamilies();
  if (!qfi.hasGraphics()) {
    throw std::runtime_error(
        "[SwapchainRenderTarget] device has no graphics queue");
  }
  queueFamily_ = qfi.graphicsQueue.value();

  createImageViews();
}

SwapchainRenderTarget::~SwapchainRenderTarget() {
  if (device_ && cmdPool_) {
    device_->waitIdle();
  }
}

void SwapchainRenderTarget::createImageViews() {
  if (!windowInfo_ || !windowInfo_->swapchain) {
    return;
  }

  auto swapInfo = windowInfo_->swapchain->getinfo();
  uint32_t count = swapInfo.imageCount;

  imageViews_.clear();
  imageViews_.reserve(count);

  std::ranges::for_each(std::views::iota(0U, count),
                        [&info = windowInfo_, &swapInfo, &views = imageViews_,
                         &device = device_](uint32_t i) {
                          auto data = info->swapchain->getSwapchainImageData(i);
                          if (!data) {
                            return;
                          }
                          vk::ImageViewCreateInfo viewInfo{};
                          viewInfo.image = data->image;
                          viewInfo.viewType = swapInfo.imageViewType;
                          viewInfo.format = data->format;
                          viewInfo.components = swapInfo.imageViewComponents;
                          viewInfo.subresourceRange =
                              swapInfo.imageViewSubresourceRange;
                          views.push_back(std::make_unique<vk::raii::ImageView>(
                              *device->getDevicePtr(), viewInfo));
                        });
}

void SwapchainRenderTarget::ensureCommandPool() {
  if (cmdPool_) {
    return;
  }
  cmdPool_ = std::make_unique<graphics::vulkan::devices::CommandBufferPool>(
      device_->getDevicePtr(), graphics::vulkan::devices::CommandPoolCreateInfo{
                                   .queueFamily = queueFamily_});
}

void SwapchainRenderTarget::releaseSlotCommand(uint32_t slot) {
  if (!cmdPool_ || slot >= pendingCmds_.size() || !pendingCmds_[slot]) {
    return;
  }

  cmdPool_->releasePrimary(std::move(*pendingCmds_[slot]));
  pendingCmds_[slot].reset();
}

void SwapchainRenderTarget::releaseAllCommands() {
  if (!cmdPool_) {
    return;
  }
  for (auto &slot : pendingCmds_) {
    if (slot) {
      cmdPool_->releasePrimary(std::move(*slot));
      slot.reset();
    }
  }
  pendingCmds_.clear();
}

graphics::vulkan::compositors::FrameContext
SwapchainRenderTarget::beginFrame() {
  graphics::vulkan::compositors::FrameContext fc;

  if (!windowInfo_ || !windowInfo_->swapchain) {
    return fc;
  }

  auto &sc = *windowInfo_->swapchain;
  if (sc.needRecreation()) {
    recreate();
    if (sc.needRecreation()) {
      return fc;
    }
  }

  ensureCommandPool();

  auto acq = sc.acquireNextImage();
  if (!acq) {
    using Code = graphics::vulkan::devices::Swapchain::PresentError::Code;
    if (acq.error().code == Code::outOfDate) {
      sc.markForRecreation();
    }
    return fc;
  }

  const uint32_t slot = sc.currentFrameSlot();
  if (slot >= pendingCmds_.size()) {
    pendingCmds_.resize(slot + 1);
  }
  releaseSlotCommand(slot);
  activeSlot_ = slot;

  const uint32_t imageIndex = *acq;
  auto data = sc.getSwapchainImageData(imageIndex);
  if (!data) {
    return {};
  }

  if (imageIndex >= imageViews_.size() || !imageViews_[imageIndex]) {
    sc.markForRecreation();
    return fc;
  }

  vk::raii::CommandBuffer cmdRaii = cmdPool_->acquirePrimary();
  vk::CommandBuffer cmd = *cmdRaii;

  cmd.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Transition to color attachment
  vk::ImageMemoryBarrier preBarrier{
      vk::AccessFlagBits::eNone,
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::QueueFamilyIgnored,
      vk::QueueFamilyIgnored,
      data->image,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      vk::DependencyFlags{}, {}, {}, preBarrier);

  // Default clear to black
  vk::RenderingAttachmentInfo clearAtt{
      *imageViews_[imageIndex],       vk::ImageLayout::eColorAttachmentOptimal,
      vk::ResolveModeFlagBits::eNone, nullptr,
      vk::ImageLayout::eUndefined,    vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore,  vk::ClearValue{defaultClearColor_}};
  vk::RenderingInfo clearRenderInfo{
      {},     vk::Rect2D{{0, 0}, data->extent}, 1, 0, 1, &clearAtt, nullptr,
      nullptr};
  cmd.beginRendering(clearRenderInfo);
  cmd.endRendering();

  fc.cmd = cmd;
  fc.image = data->image;
  fc.view = *imageViews_[imageIndex];
  fc.extent = data->extent;
  fc.format = data->format;
  fc.imageIndex = imageIndex;
  fc.valid = true;

  currentImage_ = data->image;
  currentIndex_ = imageIndex;

  pendingCmds_[slot] = std::move(cmdRaii);
  return fc;
}

void SwapchainRenderTarget::endFrame(
    graphics::vulkan::compositors::FrameContext &fc) {
  if (!fc.valid) {
    return;
  }
  if (!windowInfo_ || !windowInfo_->swapchain) {
    return;
  }

  vk::ImageMemoryBarrier postBarrier{
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::AccessFlagBits::eMemoryRead,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::QueueFamilyIgnored,
      vk::QueueFamilyIgnored,
      currentImage_,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  fc.cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                         vk::PipelineStageFlagBits::eBottomOfPipe,
                         vk::DependencyFlags{}, {}, {}, postBarrier);
  fc.cmd.end();

  vk::SubmitInfo submit{};
  submit.setCommandBuffers(fc.cmd);

  auto queue = device_->getGraphicsQueue();
  auto res = windowInfo_->swapchain->submitAndPresent(
      {.queue = queue, .family = queueFamily_},
      std::span<vk::SubmitInfo>(&submit, 1), fc.imageIndex);

  if (!res) {
    using Code = graphics::vulkan::devices::Swapchain::PresentError::Code;
    switch (res.error().code) {
    case Code::outOfDate:
      windowInfo_->swapchain->markForRecreation();
      return;
    case Code::deviceLost:
      throw std::runtime_error("Device lost during present: " +
                               res.error().message);
    default:
      throw std::runtime_error(
          res.error().message + " (code " +
          std::to_string(static_cast<int>(res.error().code)) + ")");
    }
  }

  // Suboptimal is non-fatal; mark for recreation on the next frame.
  if (*res == vk::Result::eSuboptimalKHR) {
    windowInfo_->swapchain->markForRecreation();
  }
}

void SwapchainRenderTarget::recreate() {
  if (!windowInfo_ || !windowInfo_->swapchain) {
    return;
  }

  if (device_) {
    device_->waitIdle();
  }

  // Command buffers reference the old image views; drop them.
  releaseAllCommands();
  imageViews_.clear();

  const auto info = windowInfo_->swapchain->getinfo();
  // If the surface reports currentExtent, the passed dimensions are ignored.
  // TODO it could deadlock for locking swapchain mtx outside of a swapchain
  windowInfo_->swapchain->recreateSwapchain(info.extent.width,
                                            info.extent.height);

  createImageViews();
}

} // namespace windows::sdl
