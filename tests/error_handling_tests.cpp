// error_handling_tests.cpp
// Exception safety and cancellation tests for flow::execution
// See TESTING_PLAN.md section 11

#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <stdexcept>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "exception_in_sender"_test = [] {
    auto s = just(42) | then([](int) -> int { throw std::runtime_error("test"); });

    bool error_handled = false;
    auto s2            = std::move(s) | upon_error([&](std::exception_ptr ep) {
                error_handled = true;
                try {
                  std::rethrow_exception(ep);
                } catch (const std::runtime_error& e) {
                  expect(std::string(e.what()) == "test");
                }
                return 0;
              });

    flow::this_thread::sync_wait(std::move(s2));
    expect(error_handled);
  };

  "exception_propagation_chain"_test = [] {
    bool first_handler  = false;
    bool second_handler = false;

    auto s = just(1) | then([](int) -> int { throw std::runtime_error("first"); })
             | upon_error([&](std::exception_ptr) -> int {
                 first_handler = true;
                 throw std::runtime_error("second");
               })
             | upon_error([&](std::exception_ptr) {
                 second_handler = true;
                 return -1;
               });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(first_handler);
    expect(second_handler);
    expect(std::get<0>(*result) == -1_i);
  };

  "nested_exception_handling"_test = [] {
    int error_count = 0;

    auto s = just(42) | then([](int) -> int { throw std::runtime_error("error"); })
             | upon_error([&](std::exception_ptr) {
                 error_count++;
                 return 0;
               });

    flow::this_thread::sync_wait(std::move(s));
    expect(error_count == 1_i);
  };

  "upon_stopped_handling"_test = [] {
    bool stopped_handled = false;

    auto s = just_stopped() | upon_stopped([&] {
               stopped_handled = true;
               return 42;
             });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(stopped_handled);
    expect(std::get<0>(*result) == 42_i);
  };

  "error_then_stopped"_test = [] {
    bool error_handler_called   = false;
    bool stopped_handler_called = false;

    auto s = just_error(std::make_exception_ptr(std::runtime_error("err")))
             | upon_error([&](std::exception_ptr) {
                 error_handler_called = true;
                 return -1;
               });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(error_handler_called);
    expect(std::get<0>(*result) == -1_i);
  };
}
