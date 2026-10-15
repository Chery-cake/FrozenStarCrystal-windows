#include <SDL3/SDL.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

import graphics;
import test.harness;
import windows.sdl;

#define CHECK(expr)                                                            \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg)                                                   \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

using namespace graphics::vulkan;
using namespace graphics::vulkan::compositors;

namespace {

// ── Shared state that scene copies can point at ──
struct SceneStats {
  std::atomic<int> begins{0};
  std::atomic<int> records{0};
  std::atomic<int> ends{0};

  void reset() {
    begins.store(0);
    records.store(0);
    ends.store(0);
  }
};

// ── A minimal Scene that just counts invocations ──
// Copies of this live inside each window's Compositor.  Multiple copies
// can point at the same SceneStats via shared_ptr, which is exactly how
// "the same scene" can be shared across windows.
//
// The Scene concept requires: beginRecord, record, endRecord,
// sceneContext, setSceneContext.
struct CountingScene {
  std::shared_ptr<SceneStats> stats;
  SceneRenderContext ctx{};

  explicit CountingScene(std::shared_ptr<SceneStats> s) : stats(std::move(s)) {}

  SceneRenderContext sceneContext() const { return ctx; }
  void setSceneContext(const SceneRenderContext &c) { ctx = c; }

  void beginRecord(const FrameContext &) {
    stats->begins.fetch_add(1, std::memory_order_relaxed);
  }

  devices::GPUCoroTask<void> record(const FrameContext &) {
    stats->records.fetch_add(1, std::memory_order_relaxed);
    co_return; // Suspend::Never → runs to completion immediately
  }

  void endRecord(const FrameContext &) {
    stats->ends.fetch_add(1, std::memory_order_relaxed);
  }
};

} // namespace

int main() {
  auto h = test::tryCreateHarness();
  if (!h)
    return 0;

  // ── 1. Three windows, four stats buckets ─────────────────────
  auto winA = h->makeWindow("A", 128, 128);
  auto winB = h->makeWindow("B", 128, 128);
  auto winC = h->makeWindow("C", 128, 128);

  auto sharedAB = std::make_shared<SceneStats>(); // used by A and B
  auto onlyA = std::make_shared<SceneStats>();
  auto onlyB = std::make_shared<SceneStats>();
  auto onlyC = std::make_shared<SceneStats>();

  // ── 2. Add the shared scene to both A and B ──────────────────
  // Each addScene copies the scene by value into the compositor's
  // factory lambda, but both copies point at `sharedAB`.
  SceneId idA_shared = winA->addScene(CountingScene{sharedAB});
  SceneId idB_shared = winB->addScene(CountingScene{sharedAB});

  SceneId idA_only = winA->addScene(CountingScene{onlyA});
  SceneId idB_only = winB->addScene(CountingScene{onlyB});
  SceneId idC_only = winC->addScene(CountingScene{onlyC});

  // ── 3. Scene IDs are per-compositor ─────────────────────────
  // This is a real API property worth documenting.  A's first scene and
  // B's first scene both get SceneId{1}.  That means removeScene(id) on
  // the *wrong* window could coincidentally remove a different scene.
  CHECK_MSG(idA_shared.value == 1, "A's first scene is id 1");
  CHECK_MSG(idB_shared.value == 1, "B's first scene is id 1");
  CHECK_MSG(idA_shared.value == idB_shared.value,
            "IDs are per-compositor, not global — this is a gotcha");
  CHECK_MSG(idA_only.value == 2, "A's second scene is id 2");
  CHECK_MSG(idB_only.value == 2, "B's second scene is id 2");

  // ── 4. First frame: each scene fires exactly once ────────────
  winA->executeFrame();
  winB->executeFrame();
  winC->executeFrame();
  h->windowManager->waitIdle();

  CHECK_MSG(sharedAB->records.load() == 2,
            "shared scene fired on both A and B");
  CHECK_MSG(onlyA->records.load() == 1, "A-only scene fired once");
  CHECK_MSG(onlyB->records.load() == 1, "B-only scene fired once");
  CHECK_MSG(onlyC->records.load() == 1, "C-only scene fired once");

  // ── 5. Remove the shared scene from B only ───────────────────
  sharedAB->reset();
  CHECK(winB->removeScene(idB_shared));

  winA->executeFrame();
  winB->executeFrame();
  h->windowManager->waitIdle();

  CHECK_MSG(sharedAB->records.load() == 1,
            "A still runs the shared scene after B dropped it");

  // ── 6. removeScenes(span) — remove only A's private scene ────
  std::array<SceneId, 1> aToRemove{idA_only};
  CHECK(winA->removeScenes(std::span<const SceneId>(aToRemove)));

  onlyA->reset();
  winA->executeFrame();
  h->windowManager->waitIdle();
  CHECK_MSG(onlyA->records.load() == 0, "A-only scene is gone");

  // ── 7. clearScenes() is per-window ──────────────────────────
  // Clear B; A and C must keep running.
  sharedAB->reset(); // A still owns a copy
  onlyB->reset();
  onlyC->reset();

  winB->clearScenes();
  winA->executeFrame();
  winB->executeFrame();
  winC->executeFrame();
  h->windowManager->waitIdle();

  CHECK_MSG(sharedAB->records.load() == 1,
            "A's copy of the shared scene unaffected by B's clearScenes");
  CHECK_MSG(onlyB->records.load() == 0, "B is empty");
  CHECK_MSG(onlyC->records.load() == 1, "C is untouched");

  // ── 8. Concurrent add/remove across windows ──────────────────
  // One thread per window, each hammering its own compositor.  The
  // compositors use independent shared_mutexes, so no inter-thread
  // contention on scene state — only the frame mutex on each window.
  constexpr int ROUNDS = 64;
  auto stressA = std::make_shared<SceneStats>();
  auto stressB = std::make_shared<SceneStats>();
  auto stressC = std::make_shared<SceneStats>();

  auto churn = [ROUNDS](std::shared_ptr<windows::sdl::Window> win,
                        std::shared_ptr<SceneStats> stats) {
    std::vector<SceneId> live;
    live.reserve(ROUNDS);
    for (int i = 0; i < ROUNDS; ++i) {
      if ((i % 3) == 2 && !live.empty()) {
        win->removeScene(live.back());
        live.pop_back();
      } else {
        live.push_back(win->addScene(CountingScene{stats}));
      }
    }
    for (auto id : live)
      win->removeScene(id);
  };

  std::vector<std::jthread> threads;
  threads.emplace_back(churn, winA, stressA);
  threads.emplace_back(churn, winB, stressB);
  threads.emplace_back(churn, winC, stressC);
  threads.clear(); // joins

  // Every list should now be empty: a final render touches nothing new.
  stressA->reset();
  stressB->reset();
  stressC->reset();
  winA->executeFrame();
  winB->executeFrame();
  winC->executeFrame();
  h->windowManager->waitIdle();

  CHECK_MSG(stressA->records.load() == 0, "A fully drained after churn");
  CHECK_MSG(stressB->records.load() == 0, "B fully drained after churn");
  CHECK_MSG(stressC->records.load() == 0, "C fully drained after churn");

  // ── 9. Mutate while a frame is in flight ─────────────────────
  // executeFrameAsync() snapshots sceneEntries_ under a shared lock and
  // then runs each factory outside any compositor lock.  Adding/removing
  // during that window is safe: it affects the *next* frame, not this one.
  auto midFrame = std::make_shared<SceneStats>();
  auto midId = winA->addScene(CountingScene{midFrame});

  winA->executeFrameAsync();
  winA->removeScene(midId);                                   // during flight
  auto replacement = winA->addScene(CountingScene{midFrame}); // after snapshot
  h->windowManager->waitIdle();

  // We can't predict whether the snapshot saw midId or not (the pool may
  // have already run it), but it must never be > 1 per frame.
  CHECK_MSG(midFrame->records.load() <= 1,
            "at most one mid-frame scene invocation");

  // Leave the compositor clean for the next test.
  winA->removeScene(replacement);
  h->windowManager->waitIdle();

  return test::summary("scene_sharing");
}
