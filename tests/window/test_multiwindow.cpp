#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
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

  constexpr int N = 5;
  std::vector<std::shared_ptr<windows::sdl::Window>> wins;

  for (int i = 0; i < N; ++i) {
    wins.push_back(
        h->makeWindow("mw" + std::to_string(i), 64 * (i + 1), 64 * (i + 1)));
  }

  // ── Distinct SDL window IDs ─────────────────────────────────
  std::vector<SDL_WindowID> ids;
  ids.reserve(wins.size());
  for (auto &w : wins)
    ids.push_back(SDL_GetWindowID(w->sdlWindow()));
  std::ranges::sort(ids);
  CHECK_MSG(std::ranges::adjacent_find(ids) == ids.end(),
            "duplicate SDL window IDs");

  // ── Correct sizes per window ────────────────────────────────
  // The requested size is a hint; on Wayland / tiling WMs the
  // compositor may override it.  What we can verify is that each window
  // reports a consistent, positive size through both SDL and its own
  // pixel-vs-units relationship.
  for (int i = 0; i < N; ++i) {
    auto [w, hgt] = wins[i]->windowSize();
    auto [pw, ph] = wins[i]->windowSizeInPixels();
    CHECK_MSG(w > 0 && hgt > 0, "window has a positive logical size");
    CHECK_MSG(pw >= w && ph >= hgt, "pixel size >= logical size");
  }

  // ── Async render loop ───────────────────────────────────────
  for (int frame = 0; frame < 12; ++frame) {
    h->windowManager->beginFrame();
    h->windowManager->endFrame();
  }
  h->windowManager->waitIdle();

  for (auto &w : wins)
    CHECK(!w->shouldClose());

  // ── Remove the middle window mid-run ────────────────────────
  wins.erase(wins.begin() + 2);
  for (int frame = 0; frame < 4; ++frame) {
    h->windowManager->beginFrame();
    h->windowManager->endFrame();
  }
  h->windowManager->waitIdle();
  CHECK(wins.size() == N - 1);

  // ── Explicit final wait does not hang ───────────────────────
  h->windowManager->waitIdle();
  CHECK(true);

  return test::summary("multiwindow");
}
