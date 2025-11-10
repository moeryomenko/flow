#include <flow/execution.hpp>
#include <iostream>

using namespace flow::execution;

auto main() -> int {
  // Create a thread pool with 4 threads
  thread_pool pool{4};

  // Create sender chain
  auto work = schedule(pool.get_scheduler()) | then([] -> int {
                std::cout << "Hello from thread pool!" << '\n';
                return 42;
              })
              | then([](int x) -> int {
                  std::cout << "Received value: " << x << '\n';
                  return x * 2;
                });

  // Execute and wait for result
  auto result = flow::this_thread::sync_wait(work);

  if (result) {
    std::cout << "Final result: " << std::get<0>(*result) << '\n';
  }

  return 0;
}
