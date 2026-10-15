module;

#include <SDL3/SDL.h>

#include <cstdio>
#include <memory>
#include <string>

export module test.harness;

export import std.compat;

export import graphics;
export import concurrency;

export import windows.sdl;

export namespace test {

// ─────────────────────────────────────────────────────────────
// Minimal assertion framework
// ─────────────────────────────────────────────────────────────
struct Results {
  int checks = 0;
  int failures = 0;
};

inline Results &results() {
  static Results r;
  return r;
}

inline void report(bool ok, const char *expr, const char *file, int line,
                   const char *msg) {
  auto &r = results();
  ++r.checks;
  if (!ok) {
    ++r.failures;
    if (msg != nullptr && *msg != '\0') {
      std::fprintf(stderr, "FAIL %s:%d: %s  —  %s\n", file, line, expr, msg);
    } else {
      std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
    }
  }
}

inline int summary(const char *suite) {
  auto &r = results();
  std::fprintf(stderr, "[%s] %d checks, %d failures\n", suite, r.checks,
               r.failures);
  return r.failures == 0 ? 0 : 1;
}

// ─────────────────────────────────────────────────────────────
// Harness: owns the engine stack for one test
// ─────────────────────────────────────────────────────────────
class Harness {
public:
  std::shared_ptr<graphics::vulkan::instances::Instance> instance;
  std::shared_ptr<concurrency::pool::Manager> poolManager;
  std::shared_ptr<graphics::vulkan::devices::Manager> deviceManager;
  std::shared_ptr<windows::sdl::Manager> windowManager;

  Harness() {
    instance = std::make_shared<graphics::vulkan::instances::Instance>();
    poolManager = std::make_shared<concurrency::pool::Manager>();
    deviceManager = std::make_shared<graphics::vulkan::devices::Manager>(
        instance->getInstancePtr(), poolManager);
    windowManager = std::make_shared<windows::sdl::Manager>(
        instance, deviceManager, poolManager);
  }

  ~Harness() {
    windowManager.reset();
    deviceManager.reset();
    poolManager.reset();
    instance.reset();
  }

  Harness(const Harness &) = delete;
  Harness &operator=(const Harness &) = delete;
  Harness(Harness &&) = delete;
  Harness &operator=(Harness &&) = delete;

  std::shared_ptr<windows::sdl::Window> makeWindow(const std::string &title,
                                                   uint32_t w, uint32_t h) {
    return windowManager->createWindow(title, w, h);
  }

  // How many GPUs the device manager found — useful for tests that want
  // to exercise as many devices as possible.
  size_t deviceCount() { return deviceManager->getDeviceEntries().size(); }
};

inline std::unique_ptr<Harness> tryCreateHarness() {
  try {
    return std::make_unique<Harness>();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "[harness] SKIP: %s\n", e.what());
    return nullptr;
  }
}

} // namespace test
