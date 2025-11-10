#include <flow/execution.hpp>
#include <iostream>
#include <stdexcept>

using namespace flow::execution;

auto risky_computation(int x) {
  return just(x) | then([](int val) -> int {
           std::cout << "Processing value: " << val << '\n';
           if (val < 0) {
             throw std::runtime_error("Negative value not allowed!");
           }
           return val * 2;
         })
         | upon_error([](std::exception_ptr ep) -> int {
             try {
               std::rethrow_exception(ep);
             } catch (const std::exception& e) {
               std::cout << "Error caught: " << e.what() << '\n';
               return -1;  // Default fallback value
             }
           });
}

auto main() -> int {
  std::cout << "=== Success case ===" << '\n';
  auto result1 = flow::this_thread::sync_wait(risky_computation(5));
  if (result1) {
    std::cout << "Result 1: " << std::get<0>(*result1) << '\n';
  }

  std::cout << "\n=== Error case ===" << '\n';
  auto result2 = flow::this_thread::sync_wait(risky_computation(-5));
  if (result2) {
    std::cout << "Result 2: " << std::get<0>(*result2) << '\n';
  }

  return 0;
}
