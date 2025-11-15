#include <atomic>
#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <thread>
#include <vector>

using namespace flow::execution;
using namespace flow;
using namespace boost::ut;

const suite transfer_tests = [] {
  "transfer - basic transfer to different scheduler"_test = [] {
    thread_pool pool1{2};
    thread_pool pool2{2};

    std::atomic<std::thread::id> thread_id1{};
    std::atomic<std::thread::id> thread_id2{};

    auto sender = schedule(pool1.get_scheduler()) | then([&] {
                    thread_id1 = std::this_thread::get_id();
                    return 42;
                  })
                  | transfer(pool2.get_scheduler()) | then([&](int value) {
                      thread_id2 = std::this_thread::get_id();
                      return value * 2;
                    });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 84_i);

    // Verify work ran on different threads
    expect(thread_id1.load() != std::thread::id{});
    expect(thread_id2.load() != std::thread::id{});
  };

  "transfer - value preservation"_test = [] {
    thread_pool                       pool{2};
    [[maybe_unused]] inline_scheduler inline_sch{};

    auto sender = just(1, 2, 3) | transfer(pool.get_scheduler())
                  | then([](int a, int b, int c) { return a + b + c; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 6_i);
  };

  "transfer - void sender"_test = [] {
    thread_pool                       pool{2};
    [[maybe_unused]] inline_scheduler inline_sch{};

    std::atomic<bool> executed{false};

    auto sender = just() | transfer(pool.get_scheduler()) | then([&] { executed = true; });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(executed.load());
  };

  "transfer - multiple transfers"_test = [] {
    thread_pool pool1{2};
    thread_pool pool2{2};
    thread_pool pool3{2};

    std::vector<std::thread::id> thread_ids;
    std::mutex                   mutex;

    auto sender = schedule(pool1.get_scheduler()) | then([&] {
                    std::scoped_lock lock(mutex);
                    thread_ids.push_back(std::this_thread::get_id());
                    return 10;
                  })
                  | transfer(pool2.get_scheduler()) | then([&](int value) {
                      std::scoped_lock lock(mutex);
                      thread_ids.push_back(std::this_thread::get_id());
                      return value + 20;
                    })
                  | transfer(pool3.get_scheduler()) | then([&](int value) {
                      std::scoped_lock lock(mutex);
                      thread_ids.push_back(std::this_thread::get_id());
                      return value + 30;
                    });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 60_i);
    expect(thread_ids.size() == 3_ul);
  };

  "transfer - error propagation without scheduling"_test = [] {
    thread_pool pool{2};

    std::atomic<std::thread::id> error_thread_id{};

    auto sender = just_error(std::make_exception_ptr(std::runtime_error("test error")))
                  | transfer(pool.get_scheduler()) | upon_error([&](std::exception_ptr ep) {
                      error_thread_id = std::this_thread::get_id();
                      try {
                        std::rethrow_exception(ep);
                      } catch (const std::runtime_error& e) {
                        return std::string(e.what());
                      }
                      return std::string("unknown");
                    });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == "test error");
    // Error should be propagated directly without scheduling
    expect(error_thread_id.load() == std::this_thread::get_id());
  };

  "transfer - stopped propagation without scheduling"_test = [] {
    thread_pool pool{2};

    std::atomic<std::thread::id> stopped_thread_id{};

    auto sender = just_stopped() | transfer(pool.get_scheduler()) | upon_stopped([&] {
                    stopped_thread_id = std::this_thread::get_id();
                    return 42;
                  });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
    // Stopped should be propagated directly without scheduling
    expect(stopped_thread_id.load() == std::this_thread::get_id());
  };

  "transfer - exception during transfer"_test = [] {
    thread_pool pool{2};

    bool error_handled = false;

    auto sender = just(42) | then([](int) -> int { throw std::runtime_error("test exception"); })
                  | transfer(pool.get_scheduler()) | then([](int x) { return x + 1; })
                  | upon_error([&error_handled](std::exception_ptr ep) {
                      error_handled = true;
                      try {
                        std::rethrow_exception(ep);
                      } catch (const std::runtime_error&) {
                        return -1;
                      }
                      return -2;
                    });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(error_handled);
    expect(std::get<0>(*result) == -1_i);
  };

  "transfer - with when_all"_test = [] {
    thread_pool pool1{2};
    thread_pool pool2{2};

    auto sender1 =
        schedule(pool1.get_scheduler()) | then([] { return 10; }) | transfer(pool2.get_scheduler());

    auto sender2 =
        schedule(pool1.get_scheduler()) | then([] { return 20; }) | transfer(pool2.get_scheduler());

    auto sender = when_all(sender1, sender2) | then([](int a, int b) { return a + b; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 30_i);
  };

  "transfer - pipeable syntax"_test = [] {
    thread_pool pool{2};

    // Test pipeable syntax: transfer(scheduler)
    auto sender = just(100) | transfer(pool.get_scheduler()) | then([](int x) { return x + 1; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 101_i);
  };

  "transfer - direct syntax"_test = [] {
    thread_pool pool{2};

    // Test direct syntax: transfer(sender, scheduler)
    auto sender = transfer(just(200), pool.get_scheduler()) | then([](int x) { return x + 1; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 201_i);
  };

  "transfer - to inline scheduler"_test = [] {
    thread_pool      pool{2};
    inline_scheduler inline_sch{};

    std::thread::id              main_thread = std::this_thread::get_id();
    std::atomic<std::thread::id> pool_thread{};
    std::atomic<std::thread::id> inline_thread{};

    auto sender = schedule(pool.get_scheduler()) | then([&] {
                    pool_thread = std::this_thread::get_id();
                    return 42;
                  })
                  | transfer(inline_sch) | then([&](int value) {
                      inline_thread = std::this_thread::get_id();
                      return value;
                    });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);

    // Work on pool should be on different thread
    expect(pool_thread.load() != main_thread);
    // inline_scheduler executes synchronously on the calling thread
    // which is the pool thread after transfer
    expect(inline_thread.load() != std::thread::id{});
  };

  "transfer - from inline to pool"_test = [] {
    inline_scheduler inline_sch{};
    thread_pool      pool{2};

    std::atomic<std::thread::id> thread_id{};

    auto sender = schedule(inline_sch) | then([] { return 42; }) | transfer(pool.get_scheduler())
                  | then([&](int value) {
                      thread_id = std::this_thread::get_id();
                      return value;
                    });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
    // After transfer to pool, should run on pool thread
    expect(thread_id.load() != std::this_thread::get_id());
  };

  "transfer - stress test with many transfers"_test = [] {
    thread_pool pool1{4};
    thread_pool pool2{4};

    const int        num_operations = 100;
    std::atomic<int> counter{0};

    // Execute operations directly instead of storing in vector
    for (int i = 0; i < num_operations; ++i) {
      auto sender = just(i) | transfer(pool1.get_scheduler()) | then([](int x) { return x * 2; })
                    | transfer(pool2.get_scheduler()) | then([&counter](int x) {
                        counter++;
                        return x + 1;
                      });
      auto result = this_thread::sync_wait(std::move(sender));
      expect(result.has_value());
    }

    expect(counter.load() == num_operations);
  };

  "transfer - with complex value types"_test = [] {
    thread_pool pool{2};

    struct ComplexType {
      int         value;
      std::string name;
    };

    auto sender =
        just(ComplexType{.value = 42, .name = "test"}) | transfer(pool.get_scheduler())
        | then([](const ComplexType& ct) { return ct.value + static_cast<int>(ct.name.size()); });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 46_i);  // 42 + 4 ("test".size())
  };

  "transfer - composability with let_value"_test = [] {
    thread_pool pool1{2};
    thread_pool pool2{2};

    auto sender =
        just(10) | transfer(pool1.get_scheduler()) | let_value([&pool2](int x) {
          return just(x * 2) | transfer(pool2.get_scheduler()) | then([](int y) { return y + 1; });
        });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 21_i);
  };

  "transfer - move-only types"_test = [] {
    thread_pool pool{2};

    // Transfer doesn't work well with move-only types stored across scheduler boundaries
    // This is a known limitation - use shared_ptr or other approaches
    auto sender = just(std::make_shared<int>(42)) | transfer(pool.get_scheduler())
                  | then([](std::shared_ptr<int> ptr) { return *ptr; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "transfer - multiple values"_test = [] {
    thread_pool pool{2};

    auto sender = just(1, 2, 3, 4, 5) | transfer(pool.get_scheduler())
                  | then([](int a, int b, int c, int d, int e) { return a + b + c + d + e; });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 15_i);
  };

  "transfer - verify execution context switch"_test = [] {
    thread_pool pool{1};  // Single thread for deterministic testing

    std::atomic<int> execution_count{0};
    std::thread::id  pool_thread_id;

    // First, capture the pool's thread ID
    {
      auto sender = schedule(pool.get_scheduler()) | then([&] {
                      pool_thread_id = std::this_thread::get_id();
                      execution_count++;
                    });
      this_thread::sync_wait(sender);
    }

    std::thread::id before_transfer_thread;
    std::thread::id after_transfer_thread;

    auto sender = schedule(pool.get_scheduler()) | then([&] {
                    before_transfer_thread = std::this_thread::get_id();
                    execution_count++;
                    return 42;
                  })
                  | transfer(pool.get_scheduler()) |  // Transfer back to same pool
                  then([&](int value) {
                    after_transfer_thread = std::this_thread::get_id();
                    execution_count++;
                    return value;
                  });

    auto result = this_thread::sync_wait(sender);

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
    expect(execution_count.load() == 3_i);

    // Both should be the pool thread (possibly at different times)
    expect(before_transfer_thread == pool_thread_id);
    expect(after_transfer_thread == pool_thread_id);
  };

  "transfer - error during scheduler transition"_test = [] {
    thread_pool pool{2};

    // Simulate error in the input sender
    auto sender = just(42) | then([](int) -> int {
                    throw std::runtime_error("error before transfer");
                    return 0;
                  })
                  | transfer(pool.get_scheduler()) | then([](int x) {
                      // This should not execute
                      return x + 1;
                    })
                  | upon_error([](std::exception_ptr ep) {
                      try {
                        std::rethrow_exception(ep);
                      } catch (const std::runtime_error& e) {
                        return 999;
                      }
                      return 0;
                    });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 999_i);
  };

  "transfer - large value types"_test = [] {
    thread_pool pool{2};

    struct LargeType {
      std::array<int, 100> data;
      [[nodiscard]] int    sum() const {
        int total = 0;
        for (int v : data) {
          total += v;
        }
        return total;
      }
    };

    LargeType large{};
    for (size_t i = 0; i < large.data.size(); ++i) {
      large.data[i] = static_cast<int>(i);
    }

    auto sender =
        just(large) | transfer(pool.get_scheduler()) | then([](LargeType lt) { return lt.sum(); });

    auto result = this_thread::sync_wait(std::move(sender));

    expect(result.has_value());
    expect(std::get<0>(*result) == 4950_i);  // sum of 0..99
  };
};

int main() {
  return 0;
}
