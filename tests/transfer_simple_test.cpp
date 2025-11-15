#include <flow/execution.hpp>
#include <iostream>

using namespace flow::execution;
using namespace flow;

int main() {
  std::cout << "Test 1: Basic transfer\n";

  try {
    thread_pool pool{2};

    auto sender = just(42) | transfer(pool.get_scheduler()) | then([](int x) {
                    std::cout << "Got value: " << x << '\n';
                    return x * 2;
                  });

    auto result = this_thread::sync_wait(std::move(sender));

    if (result.has_value()) {
      std::cout << "Result: " << std::get<0>(*result) << '\n';
      return 0;
    }

    std::cout << "No result\n";

    return 1;

  } catch (const std::exception& e) {
    std::cout << "Exception: " << e.what() << '\n';

    return 1;
  }
}
