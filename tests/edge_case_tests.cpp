#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <stdexcept>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "empty_sender_chain"_test = [] {
    auto s      = just();
    auto result = flow::this_thread::sync_wait(s);

    expect(result.has_value());
  };

  "nested_error_handling"_test = [] {
    auto s = just(42) | then([](int) -> int { throw std::runtime_error("error1"); })
             | upon_error([](std::exception_ptr) -> int { throw std::runtime_error("error2"); })
             | upon_error([](std::exception_ptr) { return -1; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == -1_i);
  };

  "multiple_error_channels"_test = [] {
    bool error_handler_called = false;

    auto s = just(1, 2, 3) | then([](int, int, int) -> int { throw std::runtime_error("test"); })
             | upon_error([&](std::exception_ptr) {
                 error_handler_called = true;
                 return 0;
               });

    [[maybe_unused]] auto result = flow::this_thread::sync_wait(std::move(s));

    expect(error_handler_called);
  };

  "zero_bulk_operations"_test = [] {
    int  call_count = 0;
    auto s          = just() | bulk(seq, 0, [&](std::size_t) { call_count++; });

    flow::this_thread::sync_wait(s);

    expect(call_count == 0_i);
  };

  "large_bulk_operations"_test = [] {
    const std::size_t N = 10000;
    std::atomic<int>  counter{0};

    auto s = just() | bulk(par, N, [&](std::size_t) { counter.fetch_add(1); });

    flow::this_thread::sync_wait(s);

    expect(counter.load() == static_cast<int>(N));
  };

  "chained_transformations"_test = [] {
    auto s = just(1) | then([](int x) { return x + 1; }) | then([](int x) { return x * 2; })
             | then([](int x) { return x - 1; }) | then([](int x) { return x / 2; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 1_i);  // ((1+1)*2-1)/2 = 1
  };
}
