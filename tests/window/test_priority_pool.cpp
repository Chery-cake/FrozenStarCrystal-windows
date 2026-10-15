#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

import concurrency;
import test.harness;
import windows.sdl;

#define CHECK(expr)                                                            \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, msg)                                                   \
  ::test::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, (msg))

namespace {

std::mutex g_mtx;
std::vector<int> g_order;

void record(int tag) {
  std::scoped_lock l(g_mtx);
  g_order.push_back(tag);
}

} // namespace

int main() {
  using namespace windows::sdl;

  // ── 1. Priority queue: Event pops before Background ─────────
  {
    concurrency::queues::Priority<WindowTask, std::vector, WindowTaskPolicy,
                                  concurrency::queues::NoReprioritize>
        q;

    // Push Background first so aging (if any) favours it, then Event.
    // Because aging granularity is 1 second, both have age 0 right now.
    q.push(WindowTask{[] { record(1); }, Priority::Background});
    q.push(WindowTask{[] { record(2); }, Priority::Event});

    std::stop_token st; // never requested
    concurrency::queues::Task t1, t2;
    CHECK(q.try_pop(t1, st));
    CHECK(q.try_pop(t2, st));
    t1();
    t2();

    std::scoped_lock l(g_mtx);
    CHECK_MSG(g_order.size() == 2, "both tasks ran");
    CHECK_MSG(g_order[0] == 2, "Event runs before Background");
  }

  g_order.clear();

  // ── 2. ThreadPool honours priority on a single worker ───────
  {
    WindowPool pool{1};

    std::atomic<int> done{0};
    std::atomic<bool> task0_started{false};

    // Occupy the single worker with a slow background task.  Signal when
    // we're definitely inside the task body.
    pool.submit_detach(
        [&] {
          task0_started.store(true, std::memory_order_release);
          task0_started.notify_one();

          std::this_thread::sleep_for(std::chrono::milliseconds(60));
          record(0);
          done.fetch_add(1, std::memory_order_relaxed);
        },
        Priority::Background);

    // Wait until task 0 is running on the worker.
    while (!task0_started.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    pool.submit_detach(
        [&] {
          record(1);
          done.fetch_add(1, std::memory_order_relaxed);
        },
        Priority::Background);
    pool.submit_detach(
        [&] {
          record(2);
          done.fetch_add(1, std::memory_order_relaxed);
        },
        Priority::Event);
    pool.submit_detach(
        [&] {
          record(3);
          done.fetch_add(1, std::memory_order_relaxed);
        },
        Priority::Background);

    pool.wait();

    CHECK_MSG(done.load() == 4, "all four tasks completed");

    std::scoped_lock l(g_mtx);
    CHECK_MSG(g_order.size() == 4, "four completions recorded");
    CHECK_MSG(g_order[0] == 0, "already-running task finishes first");
    CHECK_MSG(g_order[1] == 2, "Event jumps ahead of Background");
  }

  return test::summary("priority_pool");
}
