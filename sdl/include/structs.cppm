module;

#include "FrozenStarCrystal-windows_export.h"

export module windows.sdl:structs;

import std.compat;

import concurrency;

export namespace windows::sdl {

// ── Priority levels ───────────────────────────────────────────
enum class Priority : uint8_t {
  Event = 10,     // UI / input — high urgency
  Frame = 5,      // frame render
  Background = 1, // housekeeping
};

// ── Task wrapper ─────────────────────────────────────────────
// Same shape as graphics::vulkan::devices::GPUTask so the Priority queue
// can treat it uniformly: public `task` member + public `priority`.
struct FROZENSTARCRYSTAL_WINDOWS_API WindowTask {
  concurrency::queues::Task task;
  Priority priority{Priority::Background};
};

// ── Aging priority policy ────────────────────────────────────
// The Priority queue is a max-heap: the entry with the *largest* score is
// popped first. So:
//   - age (seconds waited) × promotion_per_second  → boosts older tasks
//   - + priority                                   → higher priority = more
//   urgent
//
// `epoch` is per-pool-instance (each ThreadPool owns its own policy object),
// which is fine: scores are only compared within a pool.
struct FROZENSTARCRYSTAL_WINDOWS_API WindowTaskPolicy {
  using Score = int64_t;

  static constexpr int64_t promotion_per_second = 8;
  std::chrono::steady_clock::time_point epoch =
      std::chrono::steady_clock::now();

  [[nodiscard]] Score score(const WindowTask &e) const noexcept {
    const auto age = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::steady_clock::now() - epoch)
                         .count();
    return (age * promotion_per_second) + static_cast<int64_t>(e.priority);
  }
};

// ── Concrete queue + pool types ──────────────────────────────
using WindowPoolQueue =
    concurrency::queues::Priority<WindowTask, std::vector, WindowTaskPolicy,
                                  concurrency::queues::NoReprioritize>;

using WindowPool = concurrency::pool::ThreadPool<WindowPoolQueue, WindowTask>;

} // namespace windows::sdl
