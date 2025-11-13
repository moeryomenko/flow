#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <exception>
#include <flow/execution.hpp>
#include <stdexcept>
#include <string>

using namespace boost::ut;
using namespace flow::execution;

// ============================================================================
// Test Helpers
// ============================================================================

// Helper: Sender that fails N times then succeeds
template <class T = int>
struct failing_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<T>;

  std::atomic<int>& attempt_count;
  int               fail_times;
  T                 success_value;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    return completion_signatures<set_value_t(T), set_error_t(std::exception_ptr),
                                 set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      int               fail_times;
      T                 success_value;
      R                 receiver;

      void start() & noexcept {
        int current = ++attempt_count;
        if (current <= fail_times) {
          std::move(receiver).set_error(
              std::make_exception_ptr(std::runtime_error("Attempt " + std::to_string(current))));
        } else {
          std::move(receiver).set_value(success_value);
        }
      }
    };

    return op{attempt_count, fail_times, success_value, std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      int               fail_times;
      T                 success_value;
      R                 receiver;

      void start() & noexcept {
        int current = ++attempt_count;
        if (current <= fail_times) {
          std::move(receiver).set_error(
              std::make_exception_ptr(std::runtime_error("Attempt " + std::to_string(current))));
        } else {
          std::move(receiver).set_value(success_value);
        }
      }
    };

    return op{attempt_count, fail_times, success_value, std::forward<R>(r)};
  }
};

// Helper: Sender that always fails
struct always_failing_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<int>;

  std::atomic<int>& attempt_count;
  std::string       error_message;

  template <class Env>
  auto get_completion_signatures(Env&&) const {
    return completion_signatures<set_value_t(int), set_error_t(std::exception_ptr),
                                 set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      std::string       error_message;
      R                 receiver;

      void start() & noexcept {
        ++attempt_count;
        std::move(receiver).set_error(std::make_exception_ptr(std::runtime_error(error_message)));
      }
    };

    return op{attempt_count, error_message, std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      std::string       error_message;
      R                 receiver;

      void start() & noexcept {
        ++attempt_count;
        std::move(receiver).set_error(std::make_exception_ptr(std::runtime_error(error_message)));
      }
    };

    return op{attempt_count, error_message, std::forward<R>(r)};
  }
};

// Helper: Sender that completes with stopped
struct stopped_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<>;

  std::atomic<int>& attempt_count;

  template <class Env>
  auto get_completion_signatures(Env&&) const {
    return completion_signatures<set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      R                 receiver;

      void start() & noexcept {
        ++attempt_count;
        std::move(receiver).set_stopped();
      }
    };

    return op{attempt_count, std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      R                 receiver;

      void start() & noexcept {
        ++attempt_count;
        std::move(receiver).set_stopped();
      }
    };

    return op{attempt_count, std::forward<R>(r)};
  }
};

// Helper: Sender with custom exception types
struct custom_exception : std::exception {
  int         code;
  std::string msg;

  custom_exception(int c, std::string m) : code(c), msg(std::move(m)) {}

  const char* what() const noexcept override {
    return msg.c_str();
  }
};

struct typed_failing_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<int>;

  std::atomic<int>& attempt_count;
  int               fail_times;
  int               error_code;
  int               success_value;

  template <class Env>
  auto get_completion_signatures(Env&&) const {
    return completion_signatures<set_value_t(int), set_error_t(std::exception_ptr),
                                 set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      int               fail_times;
      int               error_code;
      int               success_value;
      R                 receiver;

      void start() & noexcept {
        int current = ++attempt_count;
        if (current <= fail_times) {
          std::move(receiver).set_error(std::make_exception_ptr(
              custom_exception(error_code, "Error code " + std::to_string(error_code))));
        } else {
          std::move(receiver).set_value(success_value);
        }
      }
    };

    return op{attempt_count, fail_times, error_code, success_value, std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    struct op {
      using operation_state_concept = operation_state_t;

      std::atomic<int>& attempt_count;
      int               fail_times;
      int               error_code;
      int               success_value;
      R                 receiver;

      void start() & noexcept {
        int current = ++attempt_count;
        if (current <= fail_times) {
          std::move(receiver).set_error(std::make_exception_ptr(
              custom_exception(error_code, "Error code " + std::to_string(error_code))));
        } else {
          std::move(receiver).set_value(success_value);
        }
      }
    };

    return op{attempt_count, fail_times, error_code, success_value, std::forward<R>(r)};
  }
};

// Helper: Scheduler for backoff tests
struct test_scheduler {
  using scheduler_concept = scheduler_t;

  struct sender_t {
    using sender_concept = flow::execution::sender_t;

    template <class Env>
    auto get_completion_signatures(Env&& /*unused*/) const {
      return completion_signatures<set_value_t()>{};
    }

    template <receiver R>
    auto connect(R&& r) && {
      struct op {
        using operation_state_concept = operation_state_t;
        R    receiver;
        void start() & noexcept {
          std::move(receiver).set_value();
        }
      };
      return op{std::forward<R>(r)};
    }
  };

  [[nodiscard]] static auto schedule() {
    return sender_t{};
  }

  bool operator==(const test_scheduler&) const = default;
};

// ============================================================================
// Basic retry() Tests
// ============================================================================

int main() {
  using namespace boost::ut;

  "retry - succeeds after failures"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 3, .success_value = 42}
                | retry();
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 42));
    expect(eq(attempts.load(), 4));  // 3 failures + 1 success
  };

  "retry - immediate success"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 0, .success_value = 99}
                | retry();
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 99));
    expect(eq(attempts.load(), 1));  // Immediate success
  };

  "retry - many retries"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        failing_sender<int>{.attempt_count = attempts, .fail_times = 10, .success_value = 777}
        | retry();
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 777));
    expect(eq(attempts.load(), 11));  // 10 failures + 1 success
  };

  "retry - forwards value types correctly"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        failing_sender<std::string>{
            .attempt_count = attempts, .fail_times = 1, .success_value = std::string("success")}
        | retry();
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(std::get<0>(*result) == std::string("success"));
  };

  "retry - stopped signal propagates"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr   = stopped_sender{attempts} | retry();
    auto             result = flow::this_thread::sync_wait(sndr);

    expect(!result.has_value());     // stopped doesn't produce a value
    expect(eq(attempts.load(), 1));  // Only one attempt
  };

  "retry - pipeable syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}
                | retry();
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  "retry - function call syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        retry(failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42});
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  // ============================================================================
  // retry_n() Tests
  // ============================================================================

  "retry_n - succeeds within attempts"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 2, .success_value = 42}
                | retry_n(5);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 42));
    expect(eq(attempts.load(), 3));  // 2 failures + 1 success
  };

  "retry_n - fails after max attempts"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = always_failing_sender{.attempt_count = attempts, .error_message = "Always fails"}
                | retry_n(3) | upon_error([](std::exception_ptr) { return 0; });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 0));
    expect(eq(attempts.load(), 3));  // Max attempts reached
  };

  "retry_n - max_attempts = 1 means no retry"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = always_failing_sender{.attempt_count = attempts, .error_message = "Fail"}
                | retry_n(1) | upon_error([](std::exception_ptr) { return 0; });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(attempts.load(), 1));  // No retry
  };

  "retry_n - immediate success with retry_n"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        failing_sender<int>{.attempt_count = attempts, .fail_times = 0, .success_value = 123}
        | retry_n(10);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 123));
    expect(eq(attempts.load(), 1));  // No retries needed
  };

  "retry_n - exact max attempts"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        failing_sender<int>{.attempt_count = attempts, .fail_times = 4, .success_value = 999}
        | retry_n(5);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 999));
    expect(eq(attempts.load(), 5));  // Used all attempts
  };

  "retry_n - one short of max attempts fails"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr =
        failing_sender<int>{.attempt_count = attempts, .fail_times = 5, .success_value = 999}
        | retry_n(5) | upon_error([](std::exception_ptr) { return -1; });
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), -1));
    expect(eq(attempts.load(), 5));  // Exhausted attempts
  };

  "retry_n - pipeable syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}
                | retry_n(3);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  "retry_n - function call syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto             sndr = retry_n(
        failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}, 3);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  // ============================================================================
  // retry_if() Tests
  // ============================================================================

  "retry_if - retry on specific error"_test = [] {
    std::atomic<int> attempts{0};

    auto predicate = [](std::exception_ptr ep) {
      try {
        std::rethrow_exception(ep);
      } catch (const std::runtime_error& e) {
        return std::string(e.what()).find("Attempt") != std::string::npos;
      } catch (...) {
        return false;
      }
    };

    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 2, .success_value = 42}
                | retry_if(predicate);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 42));
    expect(eq(attempts.load(), 3));  // 2 retries + 1 success
  };

  "retry_if - no retry when predicate returns false"_test = [] {
    std::atomic<int> attempts{0};

    auto predicate = [](std::exception_ptr) {
      return false;  // Never retry
    };

    auto sndr = always_failing_sender{.attempt_count = attempts, .error_message = "Fail"}
                | retry_if(predicate) | upon_error([](std::exception_ptr) { return 0; });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(attempts.load(), 1));  // No retry
  };

  "retry_if - always retry"_test = [] {
    std::atomic<int> attempts{0};

    auto predicate = [](std::exception_ptr) {
      return true;  // Always retry
    };

    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 5, .success_value = 42}
                | retry_if(predicate);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 6));  // 5 retries + 1 success
  };

  "retry_if - retry based on error code"_test = [] {
    std::atomic<int> attempts{0};

    auto predicate = [](std::exception_ptr ep) {
      try {
        std::rethrow_exception(ep);
      } catch (const custom_exception& e) {
        return e.code == 503;  // Retry on 503 (Service Unavailable)
      } catch (...) {
        return false;
      }
    };

    auto sndr =
        typed_failing_sender{
            .attempt_count = attempts, .fail_times = 2, .error_code = 503, .success_value = 42}
        | retry_if(predicate);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 3));
  };

  "retry_if - don't retry on specific error code"_test = [] {
    std::atomic<int> attempts{0};

    auto predicate = [](std::exception_ptr ep) {
      try {
        std::rethrow_exception(ep);
      } catch (const custom_exception& e) {
        return e.code != 404;  // Don't retry on 404
      } catch (...) {
        return false;
      }
    };

    auto sndr =
        typed_failing_sender{
            .attempt_count = attempts, .fail_times = 10, .error_code = 404, .success_value = 42}
        | retry_if(predicate) | upon_error([](std::exception_ptr) { return -1; });
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), -1));
    expect(eq(attempts.load(), 1));  // No retry on 404
  };

  "retry_if - pipeable syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto             predicate = [](std::exception_ptr) { return true; };
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}
                | retry_if(predicate);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  "retry_if - function call syntax"_test = [] {
    std::atomic<int> attempts{0};
    auto             predicate = [](std::exception_ptr) { return true; };
    auto             sndr      = retry_if(
        failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42},
        predicate);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  // ============================================================================
  // retry_with_backoff() Tests
  // ============================================================================

  "retry_with_backoff - succeeds with backoff"_test = [] {
    std::atomic<int> attempts{0};
    test_scheduler   sch;

    auto start = std::chrono::steady_clock::now();
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 2, .success_value = 42}
                | retry_with_backoff(sch, std::chrono::milliseconds(10),
                                     std::chrono::milliseconds(100), 2.0, 5);
    auto result  = flow::this_thread::sync_wait(sndr);
    auto elapsed = std::chrono::steady_clock::now() - start;

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 42));
    expect(eq(attempts.load(), 3));  // 2 failures + 1 success
    // Should have some delay (at least 10ms + 20ms = 30ms)
    expect(elapsed >= std::chrono::milliseconds(25));
  };

  "retry_with_backoff - fails after max attempts"_test = [] {
    std::atomic<int> attempts{0};
    test_scheduler   sch;

    auto sndr = always_failing_sender{.attempt_count = attempts, .error_message = "Fail"}
                | retry_with_backoff(sch, std::chrono::milliseconds(5),
                                     std::chrono::milliseconds(100), 2.0, 3)
                | upon_error([](std::exception_ptr) { return -1; });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(attempts.load(), 3));
  };

  "retry_with_backoff - backoff delay increases"_test = [] {
    std::atomic<int> attempts{0};
    test_scheduler   sch;

    auto start = std::chrono::steady_clock::now();
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 3, .success_value = 42}
                | retry_with_backoff(sch, std::chrono::milliseconds(10),
                                     std::chrono::milliseconds(1000), 2.0, 5);
    auto result  = flow::this_thread::sync_wait(sndr);
    auto elapsed = std::chrono::steady_clock::now() - start;

    expect(result.has_value());
    expect(eq(attempts.load(), 4));  // 3 failures + 1 success
    // Should have delays: 10ms + 20ms + 40ms = 70ms minimum
    expect(elapsed >= std::chrono::milliseconds(60));
  };

  "retry_with_backoff - respects max delay"_test = [] {
    std::atomic<int> attempts{0};
    test_scheduler   sch;

    // With multiplier 10.0 and max_delay 50ms, should cap quickly
    auto start = std::chrono::steady_clock::now();
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 3, .success_value = 42}
                | retry_with_backoff(sch, std::chrono::milliseconds(10),
                                     std::chrono::milliseconds(50), 10.0, 5);
    auto result  = flow::this_thread::sync_wait(sndr);
    auto elapsed = std::chrono::steady_clock::now() - start;

    expect(result.has_value());
    expect(eq(attempts.load(), 4));
    // Delays: 10ms + 50ms (capped) + 50ms (capped) = 110ms minimum
    expect(elapsed >= std::chrono::milliseconds(100));
    // But not much more than that
    expect(elapsed < std::chrono::milliseconds(300));
  };

  "retry_with_backoff - pipeable syntax"_test = [] {
    std::atomic<int> attempts{0};
    test_scheduler   sch;

    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}
                | retry_with_backoff(sch, std::chrono::milliseconds(5),
                                     std::chrono::milliseconds(100), 2.0, 3);
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  // ============================================================================
  // Composition Tests
  // ============================================================================

  "retry composition - retry then transform"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 2, .success_value = 42}
                | retry() | then([](int x) { return x * 2; });
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 84));
    expect(eq(attempts.load(), 3));
  };

  "retry composition - transform then retry"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 2, .success_value = 42}
                | then([](int x) { return x + 1; }) | retry();
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(std::get<0>(*result), 43));
  };

  "retry composition - multiple retry stages"_test = [] {
    std::atomic<int> attempts{0};
    auto sndr = failing_sender<int>{.attempt_count = attempts, .fail_times = 1, .success_value = 42}
                | retry_n(3) | then([](int x) { return x; });
    auto result = flow::this_thread::sync_wait(sndr);

    expect(result.has_value());
    expect(eq(attempts.load(), 2));
  };

  // ============================================================================
  // Edge Cases and Error Handling
  // ============================================================================

  "retry - handles exception during retry"_test = [] {
    // This tests that if retry() itself throws, it's handled
    std::atomic<int> attempts{0};
    auto sndr = always_failing_sender{.attempt_count = attempts, .error_message = "Fail"}
                | retry_n(2) | upon_error([](std::exception_ptr ep) {
                    try {
                      std::rethrow_exception(ep);
                    } catch (const std::runtime_error&) {
                      return -1;
                    }
                    return -2;
                  });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(std::get<0>(*result), -1));
  };

  "retry - preserves error information"_test = [] {
    std::atomic<int> attempts{0};
    std::string      captured_error;

    auto sndr =
        always_failing_sender{.attempt_count = attempts, .error_message = "Custom error message"}
        | retry_n(1) | upon_error([&captured_error](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception& e) {
              captured_error = e.what();
            }
            return 0;
          });
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(captured_error == std::string("Custom error message"));
  };

  "retry - different value types"_test = [] {
    struct complex_type {
      int         x{};
      std::string y;
      bool        operator==(const complex_type& other) const {
        return x == other.x && y == other.y;
      }
    };

    std::atomic<int> attempts{0};
    auto             sndr = failing_sender<complex_type>{.attempt_count = attempts,
                                                         .fail_times    = 1,
                                                         .success_value = complex_type{.x = 42, .y = "hello"}}
                | retry();
    auto result = flow::this_thread::sync_wait(std::move(sndr));

    expect(result.has_value());
    expect(eq(std::get<0>(*result).x, 42));
    expect(std::get<0>(*result).y == std::string("hello"));
  };

  return 0;
}
