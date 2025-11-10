#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <string>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;
  using namespace flow::this_thread;

  "let_value_basic"_test = [] {
    auto sender = just(5) | let_value([](int x) { return just(x * 2); });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 10_i);
  };

  "let_value_chained"_test = [] {
    auto sender = just(10) | let_value([](int x) { return just(x + 5); })
                  | let_value([](int x) { return just(x * 3); });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 45_i);
  };

  "let_error_recovery"_test = [] {
    auto sender =
        just_error(std::runtime_error("test error")) | let_error([](auto) { return just(42); });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "let_stopped_fallback"_test = [] {
    auto sender = just_stopped() | let_stopped([]() { return just(99); });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 99_i);
  };

  "start_detached_basic"_test = [] {
    std::atomic<int> counter{0};

    auto sender = just(1) | then([&](int x) { counter.store(x * 10); });

    start_detached(std::move(sender));

    // Give it a moment to complete (in practice, would use proper synchronization)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    expect(counter.load() == 10_i);
  };

  "let_value_with_then"_test = [] {
    auto sender = just(3) | then([](int x) { return x + 2; })
                  | let_value([](int x) { return just(x * x); })
                  | then([](int x) { return x - 5; });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 20_i);  // (3+2)^2 - 5 = 25 - 5 = 20
  };

  "let_error_with_upon_error"_test = [] {
    int recovery_called   = 0;
    int upon_error_called = 0;

    auto sender = just_error(std::runtime_error("error")) | let_error([&](auto) {
                    recovery_called++;
                    return just(100);
                  })
                  | upon_error([&](auto) {
                      upon_error_called++;
                      return 200;
                    });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 100_i);
    expect(recovery_called == 1_i);
    expect(upon_error_called == 0_i);  // let_error recovered, so upon_error not called
  };

  "multiple_let_adaptors"_test = [] {
    auto sender = just(5) | let_value([](auto x) { return just(x * 2); })
                  | let_error([](auto) { return just(0); })
                  | let_stopped([]() { return just(-1); });

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 10_i);
  };

  "complex_composition"_test = [] {
    auto sender = just(7) | then([](int x) { return x * 2; })  // 14
                  | let_value([](int x) {                      // Chain to new sender
                      return just(x + 1) | then([](int y) { return y * 3; });
                    })                                              // (14+1)*3 = 45
                  | then([](int x) { return x / 3; })               // 15
                  | let_value([](int x) { return just(x + 10); });  // 25

    auto result = sync_wait(std::move(sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 25_i);
  };

  "explicit_type_sync_wait"_test = [] {
    // Test with explicit type specification using the helper
    auto int_sender = just(42);
    auto result1    = sync_wait.template operator()<int>()(std::move(int_sender));
    expect(result1.has_value());
    expect(std::get<0>(*result1) == 42_i);
  };

  "when_all_with_let_value"_test = [] {
    auto s1 = just(1);
    auto s2 = just(2);
    auto s3 = just(3);

    auto combined =
        when_all(std::move(s1), std::move(s2), std::move(s3))
        | then([](int a, int b, int c) { return a + b + c; });  // when_all returns tuple

    auto result = sync_wait(std::move(combined));
    expect(result.has_value());
    expect(std::get<0>(*result) == 6_i);  // 1 + 2 + 3
  };

  return 0;
}
