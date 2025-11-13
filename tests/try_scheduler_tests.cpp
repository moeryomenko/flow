#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>
#include <thread>

namespace ut = boost::ut;
using namespace ut::operators;
using namespace flow::execution;

auto main() -> int {
  using namespace ut;

  "try_scheduler_concept"_test = [] {
    // Test that schedulers implement try_scheduler concept
    static_assert(try_scheduler<decltype(std::declval<run_loop>().get_scheduler())>,
                  "run_loop scheduler should model try_scheduler");
    static_assert(try_scheduler<decltype(std::declval<thread_pool>().get_scheduler())>,
                  "thread_pool scheduler should model try_scheduler");
  };

  "would_block_t_type"_test = [] {
    // Test that would_block_t is a proper type
    would_block_t error1;
    would_block_t error2;
    expect(error1 == error2);
  };

  "try_schedule_returns_sender"_test = [] {
    run_loop loop;
    auto     sch = loop.get_scheduler();
    auto     snd = sch.try_schedule();
    static_assert(sender<decltype(snd)>, "try_schedule should return a sender");
  };

  "try_schedule_success_run_loop"_test = [] {
    run_loop loop;
    auto     sch = loop.get_scheduler();

    std::atomic<bool> executed{false};

    struct test_receiver {
      std::atomic<bool>* flag;
      using receiver_concept = receiver_t;

      void set_value() const&& noexcept {
        flag->store(true, std::memory_order_release);
      }
      void                      set_error(would_block_t /*unused*/) && noexcept {}
      void                      set_error(std::exception_ptr /*unused*/) && noexcept {}
      void                      set_stopped() && noexcept {}
      [[nodiscard]] static auto query(get_env_t /*unused*/) noexcept {
        return empty_env{};
      }
    };

    auto work = sch.try_schedule();
    auto op   = std::move(work).connect(test_receiver{&executed});
    op.start();

    // Run the loop in a separate thread
    std::thread runner([&] { loop.run(); });

    // Wait a bit for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.finish();
    runner.join();

    expect(executed.load(std::memory_order_acquire));
  };

  "try_schedule_success_thread_pool"_test = [] {
    thread_pool pool{2};
    auto        sch = pool.get_scheduler();

    std::atomic<bool> executed{false};

    struct test_receiver {
      std::atomic<bool>* flag;
      using receiver_concept = receiver_t;

      void set_value() const&& noexcept {
        flag->store(true, std::memory_order_release);
      }
      void                      set_error(would_block_t /*unused*/) && noexcept {}
      void                      set_error(std::exception_ptr /*unused*/) && noexcept {}
      void                      set_stopped() && noexcept {}
      [[nodiscard]] static auto query(get_env_t /*unused*/) noexcept {
        return empty_env{};
      }
    };

    auto work = sch.try_schedule();
    auto op   = std::move(work).connect(test_receiver{&executed});
    op.start();

    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    expect(executed.load(std::memory_order_acquire));
  };

  "try_schedule_completion_signatures"_test = [] {
    run_loop loop;
    auto     sch = loop.get_scheduler();
    auto     snd = sch.try_schedule();
    using sigs   = decltype(snd.get_completion_signatures(empty_env{}));

    // Should have set_value, set_error(would_block_t), and set_stopped
    static_assert(
        std::is_same_v<sigs, completion_signatures<set_value_t(), set_error_t(would_block_t),
                                                   set_stopped_t()>>,
        "try_schedule should have correct completion signatures");
  };

  "try_scheduler_concept_check"_test = [] {
    // Verify that regular schedulers are also try_schedulers
    run_loop    loop;
    thread_pool pool{2};

    auto run_loop_sch    = loop.get_scheduler();
    auto thread_pool_sch = pool.get_scheduler();

    // Both should satisfy scheduler concept
    static_assert(scheduler<decltype(run_loop_sch)>);
    static_assert(scheduler<decltype(thread_pool_sch)>);

    // Both should also satisfy try_scheduler concept
    static_assert(try_scheduler<decltype(run_loop_sch)>);
    static_assert(try_scheduler<decltype(thread_pool_sch)>);
  };

  "try_schedule_signal_safe_semantics"_test = [] {
    // This test verifies that try_schedule operations don't block
    thread_pool pool{4};
    auto        sch = pool.get_scheduler();

    std::atomic<bool> completed{false};

    struct immediate_receiver {
      std::atomic<bool>* flag;

      using receiver_concept = receiver_t;

      void set_value() const&& noexcept {
        flag->store(true, std::memory_order_release);
      }

      void set_error(would_block_t /*unused*/) && noexcept {
        // Would block - this is acceptable in signal handler
      }

      void set_error(std::exception_ptr /*unused*/) && noexcept {}
      void set_stopped() && noexcept {}

      [[nodiscard]] static auto query(get_env_t /*unused*/) noexcept {
        return empty_env{};
      }
    };

    auto snd = sch.try_schedule();
    auto op  = std::move(snd).connect(immediate_receiver{&completed});

    // start() should return immediately without blocking
    auto start_time = std::chrono::steady_clock::now();
    op.start();
    auto end_time = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // start() should complete very quickly (< 10ms)
    expect(duration.count() < 10_i) << "try_schedule.start() should not block";

    // Wait for potential completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  };

  return 0;
}
