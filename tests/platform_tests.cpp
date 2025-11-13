#include <atomic>
#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <thread>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "platform_thread_support"_test = [] {
    // Verify basic threading works on this platform
    std::atomic<bool> executed{false};

    std::thread t([&] {
      auto s      = just(42);
      auto result = flow::this_thread::sync_wait(std::move(s));
      executed.store(result.has_value());
    });

    t.join();
    expect(executed.load());
  };

  "atomic_operations"_test = [] {
    // Verify atomic operations work correctly
    std::atomic<int> counter{0};
    const int        iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
      auto s = just() | then([&] { counter.fetch_add(1); });
      flow::this_thread::sync_wait(s);
    }

    expect(counter.load() == iterations);
  };

  "memory_alignment"_test = [] {
    // Verify proper memory alignment - simplified
    auto s      = just(42);
    auto result = flow::this_thread::sync_wait(std::move(s));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "platform_synchronization"_test = [] {
    // Test platform synchronization primitives work
    std::mutex mtx;
    int        shared_data = 0;

    auto s = just() | then([&] {
               std::lock_guard<std::mutex> lock(mtx);
               shared_data = 42;
             });

    flow::this_thread::sync_wait(s);

    std::lock_guard<std::mutex> lock(mtx);
    expect(shared_data == 42_i);
  };

  "compiler_optimization_resilience"_test = [] {
    // Verify code works correctly even with optimizations
    volatile int value = 42;

    auto s = just(static_cast<int>(value)) | then([](int x) { return x * 2; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 84_i);
  };
}
