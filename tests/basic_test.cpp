#include <boost/ut.hpp>
#include <flow/execution.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace flow::execution;

  "just"_test = [] {
    auto s      = just(42);
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "then"_test = [] {
    auto s      = just(21) | then([](int x) { return x * 2; });
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "inline_scheduler"_test = [] {
    inline_scheduler sch;
    auto             s      = schedule(sch) | then([] { return 99; });
    auto             result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 99_i);
  };

  "error_handling"_test = [] {
    auto s = just(42) | then([](int) -> int { throw std::runtime_error("test"); })
             | upon_error([](auto) { return -1; });

    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == -1_i);
  };

  "thread_pool"_test = [] {
    thread_pool pool{2};
    auto        s      = schedule(pool.get_scheduler()) | then([] { return 123; });
    auto        result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 123_i);
  };
}
