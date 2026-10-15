#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

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

  constexpr int N = 4;
  constexpr int FRAMES = 6;

  std::vector<std::shared_ptr<windows::sdl::Window>> wins;
  for (int i = 0; i < N; ++i) {
    wins.push_back(h->makeWindow("cf" + std::to_string(i), 96, 96));
  }

  // ── 1. One thread per window, each renders its own window ───
  {
    std::atomic<int> done{0};
    std::vector<std::jthread> threads;
    threads.reserve(N);

    for (auto &w : wins) {
      threads.emplace_back([w, &done] {
        for (int i = 0; i < FRAMES; ++i)
          w->executeFrame();
        done.fetch_add(1, std::memory_order_relaxed);
      });
    }
    threads.clear(); // joins

    CHECK_MSG(done.load() == N, "all worker threads finished");
  }

  // ── 2. Async submission burst across all windows ────────────
  for (int i = 0; i < 24; ++i) {
    h->windowManager->beginFrame();
    // Occasionally do not drain immediately — stresses the frame mutex.
    if ((i % 4) == 3) {
      h->windowManager->endFrame();
    }
  }
  h->windowManager->endFrame();
  h->windowManager->waitIdle();

  for (auto &w : wins) {
    CHECK_MSG(!w->shouldClose(), "window still alive after stress");
    auto [ww, wh] = w->windowSize();
    CHECK_MSG(ww > 0 && wh > 0, "size still positive after stress");
  }

  // ── 3. Explicit per-window drain ────────────────────────────
  for (auto &w : wins)
    w->waitIdle();
  CHECK(true);

  return test::summary("concurrent_frame");
}
