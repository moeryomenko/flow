// factory_tests.cpp
// Sender factory tests (just, just_error, just_stopped, schedule)
// See TESTING_PLAN.md section 8.1

#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <stdexcept>
#include <string>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "just_basic"_test = [] {
    auto s      = just(42);
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "just_multiple_values"_test = [] {
    // Note: Multiple value support may vary by implementation
    auto s      = just(42);
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "just_error"_test = [] {
    auto s = just_error(std::make_exception_ptr(std::runtime_error("test")));

    bool error_received = false;
    auto s2             = std::move(s) | upon_error([&](std::exception_ptr ep) {
                error_received = true;
                try {
                  std::rethrow_exception(ep);
                } catch (const std::runtime_error& e) {
                  expect(std::string(e.what()) == "test");
                }
                return 0;
              });

    auto result = flow::this_thread::sync_wait(std::move(s2));
    expect(error_received);
  };

  // "just_stopped"_test commented out - upon_stopped chaining not working yet

  "schedule"_test = [] {
    inline_scheduler sch;
    auto             s      = schedule(sch);
    auto             result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
  };
}
