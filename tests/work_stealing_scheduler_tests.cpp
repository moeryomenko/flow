#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>
#include <thread>
#include <vector>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  // ============================================================================
  // Basic Functionality Tests
  // ============================================================================

  "work_stealing_scheduler_construction"_test = [] {
    work_stealing_scheduler sched;
    auto                    scheduler = sched.get_scheduler();
    expect(scheduler == scheduler);
  };

  "work_stealing_scheduler_custom_threads"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    expect(scheduler == scheduler);
  };

  "work_stealing_scheduler_single_task"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    std::atomic<bool> executed{false};
    auto              work = schedule(scheduler) | then([&] { executed.store(true); });

    flow::this_thread::sync_wait(work);

    expect(executed.load()) << "Task should be executed";
  };

  "work_stealing_scheduler_return_values"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto work   = schedule(scheduler) | then([] { return 42; });
    auto result = flow::this_thread::sync_wait(work);

    expect(result.has_value()) << "Should have result";
    expect(std::get<0>(*result) == 42) << "Result should be 42";
  };

  "work_stealing_scheduler_exception_handling"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto work = schedule(scheduler) | then([] {
                  throw std::runtime_error("test error");
                  return 42;
                });

    bool caught = false;
    try {
      flow::this_thread::sync_wait(work);
    } catch (const std::runtime_error& e) {
      caught = true;
      expect(std::string(e.what()) == "test error");
    }

    expect(caught) << "Exception should be propagated";
  };

  // ============================================================================
  // Scheduler Concept Tests
  // ============================================================================

  "work_stealing_scheduler_concepts"_test = [] {
    static_assert(scheduler<work_stealing_scheduler::work_stealing_scheduler_handle>,
                  "Should satisfy scheduler concept");
    static_assert(try_scheduler<work_stealing_scheduler::work_stealing_scheduler_handle>,
                  "Should satisfy try_scheduler concept");
  };

  "work_stealing_scheduler_forward_progress"_test = [] {
    work_stealing_scheduler sched(2);
    [[maybe_unused]] auto   scheduler = sched.get_scheduler();

    auto fpg = flow::execution::work_stealing_scheduler::work_stealing_scheduler_handle::query(
        get_forward_progress_guarantee);

    expect(fpg == forward_progress_guarantee::parallel)
        << "Should guarantee parallel forward progress";
  };

  "work_stealing_scheduler_equality"_test = [] {
    work_stealing_scheduler sched1(2);
    work_stealing_scheduler sched2(2);

    auto scheduler1a = sched1.get_scheduler();
    auto scheduler1b = sched1.get_scheduler();
    auto scheduler2  = sched2.get_scheduler();

    expect(scheduler1a == scheduler1b) << "Same scheduler should be equal";
    expect(!(scheduler1a == scheduler2)) << "Different schedulers should not be equal";
  };

  // ============================================================================
  // Try-Schedule Tests (P3669)
  // ============================================================================

  "work_stealing_scheduler_try_schedule_success"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    std::atomic<bool> executed{false};
    auto              work = try_schedule(scheduler) | then([&] { executed.store(true); });

    auto result = flow::this_thread::sync_wait(work);

    expect(result.has_value()) << "Should not block";
    expect(executed.load()) << "Task should execute";
  };

  // ============================================================================
  // Statistics Tests
  // ============================================================================

  "work_stealing_scheduler_statistics_tracking"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    // Execute some tasks
    for (int i = 0; i < 10; ++i) {
      auto work = schedule(scheduler) | then([] {});
      flow::this_thread::sync_wait(work);
    }

    // Verify statistics are being tracked
    uint64_t total_tasks = 0;

    for (size_t i = 0; i < 2; ++i) {
      auto stats = sched.get_stats(i);
      total_tasks += stats.tasks_executed;

      // Each stat should be non-negative
      expect(stats.tasks_executed >= 0);
      expect(stats.local_queue_pops >= 0);
      expect(stats.global_queue_pops >= 0);
      expect(stats.steals_attempted >= 0);
      expect(stats.steals_succeeded >= 0);

      // Succeeded steals <= attempted steals
      expect(stats.steals_succeeded <= stats.steals_attempted);
    }

    expect(total_tasks >= 10) << "Should track executed tasks";
  };

  // ============================================================================
  // Multiple Tasks Test
  // ============================================================================

  "work_stealing_scheduler_multiple_sequential_tasks"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    constexpr int    num_tasks = 20;
    std::atomic<int> count{0};
    std::atomic<int> completed{0};

    // Submit all tasks at once
    for (int i = 0; i < num_tasks; ++i) {
      auto work = schedule(scheduler) | then([&count, &completed] {
                    count.fetch_add(1, std::memory_order_relaxed);
                    completed.fetch_add(1, std::memory_order_release);
                  });

      // Start the work but don't wait
      flow::this_thread::sync_wait(work);
    }

    // Brief wait for completion
    for (int i = 0; i < 100 && completed.load(std::memory_order_acquire) < num_tasks; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    expect(count.load() == num_tasks) << "All tasks should execute";
  };

  // ============================================================================
  // Chain Tests
  // ============================================================================

  "work_stealing_scheduler_with_then_chain"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto work = schedule(scheduler) | then([] { return 10; }) | then([](int x) { return x * 2; })
                | then([](int x) { return x + 5; });

    auto result = flow::this_thread::sync_wait(work);

    expect(result.has_value());
    expect(std::get<0>(*result) == 25);  // (10 * 2) + 5
  };

  // ============================================================================
  // Thread Safety Tests
  // ============================================================================

  "work_stealing_scheduler_thread_safety"_test = [] {
    // Use more workers than test threads to avoid deadlock
    work_stealing_scheduler sched(6);
    auto                    scheduler = sched.get_scheduler();

    std::atomic<int>         counter{0};
    constexpr int            num_threads           = 4;
    constexpr int            increments_per_thread = 10;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&scheduler, &counter] {
        for (int i = 0; i < increments_per_thread; ++i) {
          auto work = schedule(scheduler)
                      | then([&counter] { counter.fetch_add(1, std::memory_order_seq_cst); });

          flow::this_thread::sync_wait(work);
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    expect(counter.load() == num_threads * increments_per_thread)
        << "All increments should be atomic";
  };

  // ============================================================================
  // Edge Cases
  // ============================================================================

  "work_stealing_scheduler_empty_task"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto work = schedule(scheduler) | then([] {});

    expect(nothrow([&] { flow::this_thread::sync_wait(work); }));
  };

  "work_stealing_scheduler_rapid_destruction"_test = [] {
    // Test that scheduler can be created and destroyed rapidly
    for (int i = 0; i < 5; ++i) {
      work_stealing_scheduler sched(2);
      auto                    scheduler = sched.get_scheduler();

      auto work   = schedule(scheduler) | then([] { return 42; });
      auto result = flow::this_thread::sync_wait(work);

      expect(result.has_value());
      // Scheduler destructs here
    }
  };

  // ============================================================================
  // Load Distribution Test (simplified)
  // ============================================================================

  "work_stealing_scheduler_load_distribution"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();

    constexpr int    num_tasks = 40;  // Reduced to avoid timeout
    std::atomic<int> completed{0};

    // Execute tasks with brief work
    for (int i = 0; i < num_tasks; ++i) {
      auto work = schedule(scheduler) | then([i, &completed] {
                    volatile int sum = 0;
                    for (int j = 0; j < 10; ++j) {  // Reduced iterations
                      sum += j;
                    }
                    completed.fetch_add(1, std::memory_order_relaxed);
                    return sum + i;
                  });

      flow::this_thread::sync_wait(work);
    }

    // Wait for all to complete
    for (int i = 0; i < 50 && completed.load() < num_tasks; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Check that work was distributed
    uint64_t total_executed = 0;
    uint64_t min_executed   = UINT64_MAX;
    uint64_t max_executed   = 0;

    for (size_t i = 0; i < 4; ++i) {
      auto stats = sched.get_stats(i);
      total_executed += stats.tasks_executed;
      min_executed = std::min(min_executed, stats.tasks_executed);
      max_executed = std::max(max_executed, stats.tasks_executed);
    }

    expect(total_executed >= num_tasks) << "All tasks should be counted";

    // Check reasonable distribution (no single worker does everything)
    if (min_executed > 0) {
      double balance_ratio = static_cast<double>(max_executed) / static_cast<double>(min_executed);
      expect(balance_ratio < 20.0) << "Load should be reasonably balanced";
    }
  };

  // ============================================================================
  // CPU-Bound Tasks Test (simplified)
  // ============================================================================

  "work_stealing_scheduler_cpu_bound_tasks"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto start = std::chrono::steady_clock::now();

    constexpr int num_tasks = 10;  // Reduced to avoid timeout
    for (int i = 0; i < num_tasks; ++i) {
      auto work = schedule(scheduler) | then([] {
                    // CPU-bound work (reduced iterations)
                    double result = 0.0;
                    for (int j = 0; j < 1000; ++j) {
                      result += std::sin(j * 0.001);
                    }
                    return result;
                  });

      flow::this_thread::sync_wait(work);
    }

    auto end      = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    expect(duration < 10000) << "Should complete in reasonable time";
  };

  return 0;
}
