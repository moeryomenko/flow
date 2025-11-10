// performance_tests.cpp
// Performance and memory benchmarks for flow::execution
// See TESTING_PLAN.md section 9

#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "sender_size"_test = [] {
    // Verify sender sizes are reasonable
    expect(sizeof(decltype(just(42))) < 128_ul);
    expect(sizeof(decltype(just(42) | then([](int x) { return x * 2; }))) < 256_ul);
  };

  "inline_scheduler_throughput"_test = [] {
    inline_scheduler sch;
    const int        iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      auto work = schedule(sch) | then([] { return 42; });
      flow::this_thread::sync_wait(std::move(work));
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Just verify it completes in reasonable time
    expect(duration.count() < 5000_i);  // Less than 5 seconds for 10k ops
  };

  "sender_chain_composition"_test = [] {
    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      auto work = just(0) | then([](int x) { return x + 1; }) | then([](int x) { return x * 2; })
                  | then([](int x) { return x - 1; }) | then([](int x) { return x / 2; });

      flow::this_thread::sync_wait(std::move(work));
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    expect(duration.count() < 2000_i);  // Less than 2 seconds for 1k chains
  };

  "bulk_parallelism"_test = [] {
    const std::size_t N = 10000;
    std::vector<int>  data(N);

    auto start = std::chrono::high_resolution_clock::now();

    auto s = just() | bulk(N, [&](std::size_t i) { data[i] = static_cast<int>(i * 2); });

    flow::this_thread::sync_wait(std::move(s));

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all operations completed
    for (std::size_t i = 0; i < N; ++i) {
      expect(data[i] == static_cast<int>(i * 2));
    }

    expect(duration.count() < 1000_i);  // Less than 1 second
  };
}
