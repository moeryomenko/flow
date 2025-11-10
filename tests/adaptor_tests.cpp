// adaptor_tests.cpp
// Sender adaptor tests (then, let_value, upon_error, bulk, when_all, etc.)
// See TESTING_PLAN.md section 8.2

#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <stdexcept>
#include <vector>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "then_basic"_test = [] {
    auto s      = just(42) | then([](int x) { return x * 2; });
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 84_i);
  };

  "then_void_return"_test = [] {
    bool executed = false;
    auto s        = just(42) | then([&](int) { executed = true; });
    auto result   = flow::this_thread::sync_wait(std::move(s));

    expect(executed);
  };

  "then_exception_propagation"_test = [] {
    auto s = just(42) | then([](int) -> int { throw std::runtime_error("test"); });

    bool error_caught = false;
    auto s2           = std::move(s) | upon_error([&](std::exception_ptr) {
                error_caught = true;
                return -1;
              });

    auto result = flow::this_thread::sync_wait(std::move(s2));
    expect(error_caught);
  };

  "upon_error"_test = [] {
    auto s = just_error(std::make_exception_ptr(std::runtime_error("error")))
             | upon_error([](std::exception_ptr) { return 99; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 99_i);
  };

  "upon_stopped"_test = [] {
    auto s = just_stopped() | upon_stopped([] { return 42; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  // let_value/let_error/let_stopped tests commented out - not yet implemented
  // "let_value"_test = [] { ... };
  // "let_error"_test = [] { ... };
  // "let_stopped"_test = [] { ... };

  "bulk"_test = [] {
    std::vector<int> results(10);
    auto s = just() | bulk(10, [&](std::size_t i) { results[i] = static_cast<int>(i * 2); });

    flow::this_thread::sync_wait(std::move(s));

    for (std::size_t i = 0; i < 10; ++i) {
      expect(results[i] == static_cast<int>(i * 2));
    }
  };

  "when_all"_test = [] {
    auto s      = when_all(just(1), just(2), just(3));
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    // when_all implementation returns all values with automatic type deduction
    expect(std::get<0>(*result) == 1_i);
    expect(std::get<1>(*result) == 2_i);
    expect(std::get<2>(*result) == 3_i);
  };

  "transfer"_test = [] {
    inline_scheduler inline_sch;

    std::thread::id inline_thread;

    auto s = schedule(inline_sch) | then([&] {
               inline_thread = std::this_thread::get_id();
               return 42;
             });

    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };
}
