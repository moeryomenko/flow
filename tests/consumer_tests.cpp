// consumer_tests.cpp
// Sender consumer tests (sync_wait, start_detached, etc.)
// See TESTING_PLAN.md section 8.3

#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>
#include <thread>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "sync_wait"_test = [] {
    auto s      = just(42);
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "sync_wait_with_transformation"_test = [] {
    auto s      = just(21) | then([](int x) { return x * 2; });
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "sync_wait_error_handling"_test = [] {
    auto s = just_error(std::make_exception_ptr(std::runtime_error("test")))
             | upon_error([](std::exception_ptr) { return 99; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 99_i);
  };

  // start_detached tests commented out - not yet implemented in library
  // "start_detached"_test = [] { ... };
}
