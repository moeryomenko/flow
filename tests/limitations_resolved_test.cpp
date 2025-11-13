#include <boost/ut.hpp>
#include <flow/execution.hpp>

using std::move;

int main() {
  using namespace boost::ut;
  using namespace flow::execution;
  using namespace flow::this_thread;

  "when_all_value_aggregation"_test = [] {
    // Test that when_all aggregates values from all senders with automatic type deduction
    auto s1 = just(10);
    auto s2 = just(20);
    auto s3 = just(30);

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));
    auto result   = sync_wait(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == 10_i);
    expect(std::get<1>(*result) == 20_i);
    expect(std::get<2>(*result) == 30_i);
  };

  "when_all_with_transformations"_test = [] {
    auto s1 = just(5) | then([](int x) { return x * 2; });
    auto s2 = just(7) | then([](int x) { return x + 3; });
    auto s3 = just(9) | then([](int x) { return x - 1; });

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));
    auto result   = sync_wait(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == 10_i);  // 5 * 2
    expect(std::get<1>(*result) == 10_i);  // 7 + 3
    expect(std::get<2>(*result) == 8_i);   // 9 - 1
  };

  "when_all_then_chain"_test = [] {
    auto s1 = just(1);
    auto s2 = just(2);
    auto s3 = just(3);

    auto sum_sender = when_all(std::move(s1), std::move(s2), std::move(s3))
                      | then([](int a, int b, int c) { return a + b + c; });

    auto result = sync_wait(std::move(sum_sender));
    expect(result.has_value());
    expect(std::get<0>(*result) == 6_i);
  };

  "automatic_type_deduction_int"_test = [] {
    // sync_wait automatically deduces int from just()
    auto sender = just(42);
    auto result = sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "automatic_type_deduction_chain"_test = [] {
    auto sender = just(10) | then([](int x) { return x * 2; });
    auto result = sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 20_i);
  };

  "automatic_type_deduction_when_all"_test = [] {
    // sync_wait automatically deduces tuple<int, int, int> from when_all
    auto combined = when_all(just(1), just(2), just(3));
    auto result   = sync_wait(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == 1_i);
    expect(std::get<1>(*result) == 2_i);
    expect(std::get<2>(*result) == 3_i);
  };

  "thread_pool_scheduler_works"_test = [] {
    thread_pool pool(2);
    auto        sched = pool.get_scheduler();

    std::atomic<int> counter{0};
    auto             sender = sched.schedule() | then([&]() {
                    counter.fetch_add(1);
                    return 42;
                  });

    auto result = sync_wait(sender);
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
    expect(counter.load() == 1_i);
  };

  "run_loop_scheduler_works"_test = [] {
    run_loop loop;
    auto     sched = loop.get_scheduler();

    std::atomic<int> value{0};

    // Launch a task on the run_loop
    std::thread worker([&]() {
      auto sender = sched.schedule() | then([&]() { value.store(42); });
      start_detached(std::move(sender));
      loop.run();
    });

    // Give it time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.finish();
    worker.join();

    expect(value.load() == 42_i);
  };

  "complex_when_all_pipeline"_test = [] {
    auto pipeline = when_all(just(1) | then([](int x) { return x * 10; }),
                             just(2) | then([](int x) { return x * 20; }),
                             just(3) | then([](int x) { return x * 30; }))
                    | then([](int a, int b, int c) { return a + b + c; });

    auto result = sync_wait(std::move(pipeline));
    expect(result.has_value());
    expect(std::get<0>(*result) == 140_i);  // 10 + 40 + 90
  };

  "when_all_with_let_value"_test = [] {
    auto pipeline = when_all(just(2), just(3), just(4))
                    | let_value([](int a, int b, int c) { return just(a * b * c); });

    auto result = sync_wait(std::move(pipeline));
    expect(result.has_value());
    expect(std::get<0>(*result) == 24_i);  // 2 * 3 * 4
  };

  return 0;
}
