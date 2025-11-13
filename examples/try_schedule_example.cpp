#include <atomic>
#include <flow/execution.hpp>
#include <iostream>

// Example demonstrating P3669R2 non-blocking support
// This shows how try_schedule can be used in scenarios where blocking is not acceptable,
// such as signal handlers or real-time contexts.

using namespace flow::execution;

int main() {
  std::cout << "=== P3669R2 Non-Blocking Support Example ===\n\n";

  thread_pool pool{4};
  auto        sch = pool.get_scheduler();

  // Example 1: Basic try_schedule usage
  std::cout << "1. Basic try_schedule usage:\n";
  {
    std::atomic<bool> executed{false};

    auto work = sch.try_schedule() | then([&] {
                  executed.store(true);
                  return 42;
                });

    auto result = flow::this_thread::sync_wait(work);

    if (result) {
      std::cout << "   Result: " << std::get<0>(*result) << "\n";
    } else {
      std::cout << "   No result (error or stopped)\n";
    }
  }

  // Example 2: Basic usage with then
  std::cout << "\n2. Composing try_schedule with then:\n";
  {
    auto work = sch.try_schedule() | then([] {
                  std::cout << "   Primary work executed\n";
                  return 100;
                });

    auto result = flow::this_thread::sync_wait(work);

    if (result) {
      std::cout << "   Final result: " << std::get<0>(*result) << "\n";
    }
  }

  // Example 3: Direct scheduler usage
  std::cout << "\n3. Direct try_schedule from scheduler:\n";
  {
    std::atomic<int> count{0};

    for (int i = 0; i < 5; ++i) {
      auto work = sch.try_schedule() | then([&, i] {
                    count.fetch_add(1);
                    return i;
                  });

      auto result = flow::this_thread::sync_wait(work);
      if (result) {
        std::cout << "   Task " << std::get<0>(*result) << " completed\n";
      }
    }

    std::cout << "   Total: " << count.load() << " tasks completed\n";
  }

  // Example 4: Comparison with regular schedule
  std::cout << "\n4. Difference between schedule and try_schedule:\n";
  {
    std::cout << "   schedule():     May block during enqueue (uses mutex)\n";
    std::cout << "   try_schedule(): Never blocks, returns would_block error if necessary\n";
    std::cout << "   Use try_schedule in:\n";
    std::cout << "     - Signal handlers\n";
    std::cout << "     - Real-time contexts\n";
    std::cout << "     - Lock-free algorithms\n";
    std::cout << "     - Interrupt handlers\n";
  }

  // Example 5: Checking for try_scheduler support
  std::cout << "\n5. Checking scheduler capabilities:\n";
  {
    run_loop loop;

    std::cout << "   run_loop supports try_schedule: "
              << (try_scheduler<decltype(loop.get_scheduler())> ? "Yes" : "No") << "\n";
    std::cout << "   thread_pool supports try_schedule: "
              << (try_scheduler<decltype(sch)> ? "Yes" : "No") << "\n";
  }

  std::cout << "\n=== Example Complete ===\n";
  return 0;
}
