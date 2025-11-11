#include <chrono>
#include <flow/execution.hpp>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>
#include <variant>

using namespace flow::execution;

// Example 1: Basic when_any with immediate values
void example_basic() {
  std::cout << "=== Example 1: Basic when_any ===\n";

  auto s1 = just(42);
  auto s2 = just(100);
  auto s3 = just(200);

  auto any_sender = when_any(std::move(s1), std::move(s2), std::move(s3));
  auto result     = flow::this_thread::sync_wait(std::move(any_sender));

  if (result) {
    auto [index] = *result;
    std::cout << "Completed sender index: " << index << "\n\n";
  }
}

// Example 2: Racing asynchronous operations
void example_racing() {
  std::cout << "=== Example 2: Racing async operations ===\n";

  thread_pool pool{4};

  // Create three tasks with different delays
  auto fast_task = schedule(pool.get_scheduler()) | then([] {
                     std::this_thread::sleep_for(std::chrono::milliseconds(10));
                     std::cout << "Fast task completed!\n";
                     return "fast";
                   });

  auto medium_task = schedule(pool.get_scheduler()) | then([] {
                       std::this_thread::sleep_for(std::chrono::milliseconds(50));
                       std::cout << "Medium task completed!\n";
                       return "medium";
                     });

  auto slow_task = schedule(pool.get_scheduler()) | then([] {
                     std::this_thread::sleep_for(std::chrono::milliseconds(100));
                     std::cout << "Slow task completed!\n";
                     return "slow";
                   });

  auto race   = when_any(fast_task, medium_task, slow_task);
  auto result = flow::this_thread::sync_wait(std::move(race));

  if (result) {
    auto [index] = *result;
    std::cout << "Winner index: " << index << "\n";
    std::cout << "(0=fast, 1=medium, 2=slow)\n\n";
  }
}

// Example 3: Mixed types with when_any
void example_mixed_types() {
  std::cout << "=== Example 3: Mixed types ===\n";

  auto s1 = just(42);
  auto s2 = just(std::string("Hello, when_any!"));
  auto s3 = just(std::numbers::pi);

  auto any_sender = when_any(std::move(s1), std::move(s2), std::move(s3));
  auto result     = flow::this_thread::sync_wait(std::move(any_sender));

  if (result) {
    auto [variant_result] = *result;
    std::cout << "Completed with variant (index: " << variant_result.index() << ")\n";
    std::cout << "(0=int, 1=string, 2=double)\n";

    // Visit the variant to get the actual value
    std::visit(
        [](auto&& tuple_val) {
          if constexpr (std::tuple_size_v<std::decay_t<decltype(tuple_val)>> > 0) {
            std::cout << "Value: " << std::get<0>(tuple_val) << "\n\n";
          }
        },
        variant_result);
  }
}

// Example 4: Timeout pattern with when_any
void example_timeout_pattern() {
  std::cout << "=== Example 4: Timeout pattern ===\n";

  thread_pool pool{4};

  // Long-running task
  auto long_task = schedule(pool.get_scheduler()) | then([] {
                     std::this_thread::sleep_for(std::chrono::milliseconds(200));
                     return "task completed";
                   });

  // Timeout task
  auto timeout = schedule(pool.get_scheduler()) | then([] {
                   std::this_thread::sleep_for(std::chrono::milliseconds(50));
                   return "timeout!";
                 });

  auto race   = when_any(long_task, timeout);
  auto result = flow::this_thread::sync_wait(std::move(race));

  if (result) {
    auto [index] = *result;
    if (index == nullptr) {
      std::cout << "Task finished!\n";
    } else {
      std::cout << "Operation timed out!\n";
    }
    std::cout << "\n";
  }
}

// Example 5: Combining when_any with other adaptors
void example_composition() {
  std::cout << "=== Example 5: Composition with other adaptors ===\n";

  thread_pool pool{4};

  auto s1 = schedule(pool.get_scheduler()) | then([] { return 10; });
  auto s2 = schedule(pool.get_scheduler()) | then([] { return 20; });
  auto s3 = schedule(pool.get_scheduler()) | then([] { return 30; });

  // Use when_any and then transform the result
  auto pipeline = when_any(s1, s2, s3) | then([](std::size_t index) {
                    std::cout << "Winner from index " << index << "\n";
                    return index * 100;
                  });

  auto result = flow::this_thread::sync_wait(std::move(pipeline));
  if (result) {
    auto [value] = *result;
    std::cout << "Pipeline completed with: " << value << "\n\n";
  }
}

// Example 6: Multiple parallel alternatives
void example_parallel_alternatives() {
  std::cout << "=== Example 6: Multiple parallel alternatives ===\n";

  thread_pool pool{4};

  // Simulate different strategies to solve a problem
  auto strategy_a = schedule(pool.get_scheduler()) | then([] {
                      std::this_thread::sleep_for(std::chrono::milliseconds(30));
                      return "Strategy A: Quick approximation";
                    });

  auto strategy_b = schedule(pool.get_scheduler()) | then([] {
                      std::this_thread::sleep_for(std::chrono::milliseconds(60));
                      return "Strategy B: Balanced approach";
                    });

  auto strategy_c = schedule(pool.get_scheduler()) | then([] {
                      std::this_thread::sleep_for(std::chrono::milliseconds(100));
                      return "Strategy C: Precise solution";
                    });

  std::cout << "Starting parallel strategies...\n";
  auto race   = when_any(strategy_a, strategy_b, strategy_c);
  auto result = flow::this_thread::sync_wait(std::move(race));

  if (result) {
    auto [index] = *result;
    std::cout << "First strategy to complete (index " << index << ")\n";
    std::cout << "(0=A, 1=B, 2=C)\n\n";
  }
}

int main() {
  std::cout << "when_any Algorithm Examples\n";
  std::cout << "============================\n\n";

  example_basic();
  example_racing();
  example_mixed_types();
  example_timeout_pattern();
  example_composition();
  example_parallel_alternatives();

  std::cout << "All examples completed!\n";
  return 0;
}
