#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

import graphics;
import graphics.vulkan.shaders;   // Shader, EntryPoint
import graphics.vulkan.pipelines; // DynamicPipelineInfo
import test.harness;
import windows.sdl;

#define CHECK(expr)                                                            \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg)                                                   \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

using namespace graphics::vulkan;
using namespace graphics::vulkan::compositors;
using namespace graphics::vulkan::pipelines;

namespace {

// ── A Scene that draws one triangle per frame ─────────────────
// Satisfies the compositors::Scene concept: beginRecord, record,
// endRecord, sceneContext, setSceneContext.
struct TriangleScene {
  shaders::Shader shader{
      .entryPoints =
          {
              {.entry = "vs_main", .type = vk::ShaderStageFlagBits::eVertex},
              {.entry = "fs_main", .type = vk::ShaderStageFlagBits::eFragment},
          },
      .sourcePath = "triangle.slang",
  };

  SceneRenderContext ctx{};
  std::shared_ptr<pipelines::Manager> pipelineMgr;
  std::shared_ptr<vk::raii::Pipeline> pipeline;

  // ── Scene concept surface ──────────────────────────────────
  SceneRenderContext sceneContext() const { return ctx; }
  void setSceneContext(const SceneRenderContext &c) { ctx = c; }

  void beginRecord(const FrameContext &frame) {
    if (pipeline)
      return;

    DynamicPipelineInfo info{};
    info.tag.shaderTag = &shader;
    info.attachments.color = {frame.format};

    auto &dev = *pipelineMgr; // placeholder; real code passes Device
    (void)dev;
    // (See note below — the pipeline manager needs the vk::raii::Device.)
  }

  devices::GPUCoroTask<void> record(const FrameContext &frame) {
    // Draw three vertices with no vertex buffer (SV_VertexID).
    // Full implementation omitted — see note below.
    co_return;
  }

  void endRecord(const FrameContext &frame) {}
};

} // namespace

int main() {
  auto h = test::tryCreateHarness();
  if (!h)
    return 0;

  auto win = h->makeWindow("triangle", 256, 256);

  // Shader compilation requires the shader Manager, which currently
  // lives per-compositor in graphics; the Window does not expose it.
  // See the "what's missing" section below.

  for (int i = 0; i < 8; ++i) {
    win->executeFrame();
  }
  h->windowManager->waitIdle();
  CHECK(!win->shouldClose());

  return test::summary("triangle");
}
