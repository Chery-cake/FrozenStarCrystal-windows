#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <vector>

import test.harness;
import windows.sdl;

#define CHECK(expr)                                                            \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg)                                                   \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

namespace {

// Tiny per-window counter that an onEvent callback writes into.
struct Counter {
  std::atomic<int> keyEvents{0};
  std::atomic<int> quitEvents{0};
  std::atomic<int> mouseEvents{0};
};

void attach(std::shared_ptr<windows::sdl::Window> w,
            std::shared_ptr<Counter> c) {
  w->onEvent([c](const SDL_Event &e) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      c->keyEvents.fetch_add(1, std::memory_order_relaxed);
      break;
    case SDL_EVENT_QUIT:
      c->quitEvents.fetch_add(1, std::memory_order_relaxed);
      break;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      c->mouseEvents.fetch_add(1, std::memory_order_relaxed);
      break;
    default:
      break;
    }
  });
}

} // namespace

int main() {
  auto h = test::tryCreateHarness();
  if (!h)
    return 0;

  auto a = h->makeWindow("A", 128, 128);
  auto b = h->makeWindow("B", 128, 128);

  auto ca = std::make_shared<Counter>();
  auto cb = std::make_shared<Counter>();
  attach(a, ca);
  attach(b, cb);

  const SDL_WindowID idA = SDL_GetWindowID(a->sdlWindow());
  const SDL_WindowID idB = SDL_GetWindowID(b->sdlWindow());
  CHECK(idA != idB);

  // ── 1. Key events targeted at A ─────────────────────────────
  for (int i = 0; i < 3; ++i) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.windowID = idA;
    e.key.key = SDLK_A;
    e.key.down = true;
    SDL_PushEvent(&e);
  }
  h->windowManager->pollEvents();
  h->windowManager->waitIdle();

  CHECK_MSG(ca->keyEvents.load() == 3, "A should have 3 key events");
  CHECK_MSG(cb->keyEvents.load() == 0, "B should have 0 key events");

  // ── 2. Key events targeted at B ─────────────────────────────
  {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_UP;
    e.key.windowID = idB;
    e.key.key = SDLK_B;
    e.key.down = false;
    SDL_PushEvent(&e);
  }
  h->windowManager->pollEvents();
  h->windowManager->waitIdle();

  CHECK_MSG(cb->keyEvents.load() == 1, "B should have 1 key event");
  CHECK_MSG(ca->keyEvents.load() == 3, "A key count unchanged");

  // ── 3. Mouse events targeted at A ───────────────────────────
  {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = idA;
    e.motion.x = 10;
    e.motion.y = 20;
    SDL_PushEvent(&e);
  }
  h->windowManager->pollEvents();
  h->windowManager->waitIdle();

  CHECK_MSG(ca->mouseEvents.load() == 1, "A should have 1 mouse event");
  CHECK_MSG(cb->mouseEvents.load() == 0, "B should have 0 mouse events");

  // ── 4. SDL_EVENT_QUIT broadcasts to every window ────────────
  {
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
  }
  h->windowManager->pollEvents();
  h->windowManager->waitIdle();

  CHECK_MSG(ca->quitEvents.load() == 1, "A got quit");
  CHECK_MSG(cb->quitEvents.load() == 1, "B got quit");
  CHECK_MSG(a->shouldClose(), "A shouldClose() true after QUIT");
  CHECK_MSG(b->shouldClose(), "B shouldClose() true after QUIT");

  return test::summary("event_routing");
}
