#include <SDL3/SDL.h>

#include <cstdio>
#include <memory>
#include <string>

import test.harness;
import windows.sdl;

#define CHECK(expr)                                                            \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg)                                                   \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

int main() {
  auto h = test::tryCreateHarness();
  if (!h)
    return 0;

  std::fprintf(stderr, "[lifecycle] devices: %zu\n", h->deviceCount());

  // ── 1. Empty manager ────────────────────────────────────────
  h->windowManager->pollEvents();
  h->windowManager->beginFrame();
  h->windowManager->endFrame();
  h->windowManager->waitIdle();
  CHECK(true);

  // ── 2. Single window, sync frame ────────────────────────────
  {
    auto w = h->makeWindow("solo", 320, 240);
    CHECK(w != nullptr);
    CHECK(w->sdlWindow() != nullptr);
    CHECK(!w->shouldClose());

    auto [ww, wh] = w->windowSize();
    CHECK_MSG(ww > 0, "logical width must be positive");
    CHECK_MSG(wh > 0, "logical height must be positive");

    w->executeFrame();
    h->windowManager->waitIdle();
  }

  // ── 3. Many windows, async frames ───────────────────────────
  {
    std::vector<std::shared_ptr<windows::sdl::Window>> wins;
    for (int i = 0; i < 4; ++i) {
      wins.push_back(h->makeWindow("async" + std::to_string(i), 128, 128));
    }

    for (int f = 0; f < 8; ++f) {
      h->windowManager->beginFrame();
      h->windowManager->endFrame();
    }
    h->windowManager->waitIdle();

    for (auto &w : wins)
      CHECK(!w->shouldClose());
  }

  // ── 4. Out-of-order destruction ─────────────────────────────
  {
    auto a = h->makeWindow("a", 64, 64);
    auto b = h->makeWindow("b", 64, 64);
    auto c = h->makeWindow("c", 64, 64);
    b.reset();
    c.reset();
    a.reset();
    h->windowManager->waitIdle();
    CHECK(true);
  }

  return test::summary("lifecycle");
}
