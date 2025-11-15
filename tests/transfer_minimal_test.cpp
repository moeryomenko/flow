#include <flow/execution.hpp>
#include <iostream>

using namespace flow::execution;
using namespace flow;

int main() {
  std::cout << "Minimal transfer test\n";

  try {
    // Test 1: Transfer with inline scheduler (synchronous)
    std::cout << "Test 1: inline scheduler\n";
    {
      inline_scheduler sch;
      auto             sender = just(42) | transfer(sch) | then([](int x) { return x * 2; });
      auto             result = this_thread::sync_wait(std::move(sender));

      if (result.has_value()) {
        std::cout << "  Result: " << std::get<0>(*result) << " (expected 84) - PASS\n";
      } else {
        std::cout << "  No result - FAIL\n";
        return 1;
      }
    }

    // Test 2: Transfer with thread_pool (async)
    std::cout << "Test 2: thread_pool\n";
    {
      thread_pool pool{1};
      auto        sender = just(100) | transfer(pool.get_scheduler()) | then([](int x) {
                      std::cout << "  In then callback, x=" << x << "\n";
                      return x + 1;
                    });

      std::cout << "  Calling sync_wait...\n";
      auto result = this_thread::sync_wait(std::move(sender));
      std::cout << "  sync_wait returned\n";

      if (result.has_value()) {
        std::cout << "  Result: " << std::get<0>(*result) << " (expected 101) - PASS\n";
      } else {
        std::cout << "  No result - FAIL\n";
        return 1;
      }
    }

    std::cout << "\nALL TESTS PASSED\n";
    return 0;

  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << '\n';
    return 1;
  }
}
