#include <atomic>
#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <thread>
#include <vector>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "thread_safety_atomic_counter"_test = [] {
    std::atomic<int> counter{0};
    const int        num_threads           = 10;
    const int        increments_per_thread = 100;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
      threads.emplace_back([&] {
        for (int j = 0; j < increments_per_thread; ++j) {
          auto s = just() | then([&] { counter.fetch_add(1); });
          flow::this_thread::sync_wait(s);
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    expect(counter.load() == num_threads * increments_per_thread);
  };

  "concurrent_sender_execution"_test = [] {
    std::atomic<int> completed{0};
    const int        num_operations = 100;

    std::vector<std::thread> threads;
    threads.reserve(num_operations);
    for (int i = 0; i < num_operations; ++i) {
      threads.emplace_back([&] {
        auto s      = just(42) | then([](int x) { return x * 2; });
        auto result = flow::this_thread::sync_wait(std::move(s));
        if (result.has_value()) {
          completed.fetch_add(1);
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    expect(completed.load() == num_operations);
  };

  "no_data_races_on_shared_state"_test = [] {
    std::atomic<int> shared_value{0};
    const int        num_iterations = 1000;

    for (int i = 0; i < num_iterations; ++i) {
      auto s = just() | then([&] {
                 int old = shared_value.load();
                 shared_value.store(old + 1);
               });
      flow::this_thread::sync_wait(s);
    }

    expect(shared_value.load() == num_iterations);
  };
}
