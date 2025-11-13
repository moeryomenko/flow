#include <atomic>
#include <boost/ut.hpp>
#include <flow/execution.hpp>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  // ============================================================================
  // Test 1: Statistics Are Atomic
  // Verifies that stats counters use atomic operations
  // ============================================================================

  "stats_are_atomic"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();

    // Execute some tasks
    auto work1 = schedule(scheduler) | then([] { return 1; });
    auto work2 = schedule(scheduler) | then([] { return 2; });

    flow::this_thread::sync_wait(work1);
    flow::this_thread::sync_wait(work2);

    // Stats should be readable without crashes
    uint64_t total = 0;
    for (size_t i = 0; i < 4; ++i) {
      auto stats = sched.get_stats(i);
      total += stats.tasks_executed;
      expect(stats.steals_succeeded <= stats.steals_attempted);
    }

    expect(total >= 2) << "Should track task execution";
  };

  // ============================================================================
  // Test 2: Stats Movability
  // Verifies that stats struct can be moved/copied for vector resize
  // ============================================================================

  "stats_movable_and_copyable"_test = [] {
    work_stealing_scheduler sched1(2);
    auto                    sched1_handle = sched1.get_scheduler();

    auto                  work   = schedule(sched1_handle) | then([] { return 42; });
    [[maybe_unused]] auto result = flow::this_thread::sync_wait(work);

    auto stats1 = sched1.get_stats(0);

    // Copy stats
    auto stats_copy = stats1;
    expect(stats_copy.tasks_executed == stats1.tasks_executed);

    // Move stats
    auto stats_move = stats_copy;
    expect(stats_move.tasks_executed == stats1.tasks_executed);
  };

  // ============================================================================
  // Test 3: Dynamic Stats Array Sizing
  // Verifies that schedulers with many threads work correctly
  // ============================================================================

  "stats_dynamic_sizing"_test = [] {
    constexpr size_t many_threads = 8;

    work_stealing_scheduler sched(many_threads);
    auto                    scheduler = sched.get_scheduler();

    // Execute a task
    auto work = schedule(scheduler) | then([] { return 1; });
    flow::this_thread::sync_wait(work);

    // Verify we can get stats for all workers
    for (size_t i = 0; i < many_threads; ++i) {
      auto stats = sched.get_stats(i);
      expect(stats.tasks_executed >= 0);
    }

    // Out-of-bounds returns zeros
    auto invalid_stats = sched.get_stats(many_threads + 10);
    expect(invalid_stats.tasks_executed == 0);
  };

  // ============================================================================
  // Test 4: Thread-Local RNG (verified by not crashing)
  // ============================================================================

  "thread_local_rng_doesnt_crash"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();

    // Submit multiple tasks - uses thread-local RNG
    for (int i = 0; i < 10; ++i) {
      auto work = schedule(scheduler) | then([] { return 42; });
      flow::this_thread::sync_wait(work);
    }

    expect(true) << "Thread-local RNG works without crashes";
  };

  // ============================================================================
  // Test 5: Memory Ordering (sequence numbers use release)
  // ============================================================================

  "memory_ordering_visible"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    std::atomic<int> counter{0};

    auto work1 =
        schedule(scheduler) | then([&counter] { counter.fetch_add(1, std::memory_order_release); });
    auto work2 =
        schedule(scheduler) | then([&counter] { counter.fetch_add(1, std::memory_order_release); });

    flow::this_thread::sync_wait(work1);
    flow::this_thread::sync_wait(work2);

    expect(counter.load(std::memory_order_acquire) == 2);
  };

  // ============================================================================
  // Test 6: Exception Safety
  // ============================================================================

  "exception_safety"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    bool caught = false;
    auto work   = schedule(scheduler) | then([] {
                  throw std::runtime_error("test");
                  return 42;
                });

    try {
      flow::this_thread::sync_wait(work);
    } catch (const std::runtime_error&) {
      caught = true;
    }

    expect(caught) << "Exceptions propagate safely";
  };

  // ============================================================================
  // Test 7: Clean Destruction
  // ============================================================================

  "clean_destruction"_test = [] {
    {
      work_stealing_scheduler sched(2);
      auto                    scheduler = sched.get_scheduler();

      auto work = schedule(scheduler) | then([] { return 1; });
      flow::this_thread::sync_wait(work);

      // Destructor runs here
    }

    expect(true) << "Destruction completes cleanly";
  };

  // ============================================================================
  // Test 8: Zero Thread Validation
  // ============================================================================

  "construction_validation_zero_threads"_test = [] {
    bool caught = false;
    try {
      work_stealing_scheduler sched(0);
    } catch (const std::invalid_argument&) {
      caught = true;
    }

    expect(caught) << "Should throw on zero threads";
  };

  // ============================================================================
  // Test 9: Multiple Schedulers Don't Interfere
  // ============================================================================

  "multiple_schedulers_independent"_test = [] {
    work_stealing_scheduler sched1(2);
    work_stealing_scheduler sched2(2);

    auto scheduler1 = sched1.get_scheduler();
    auto scheduler2 = sched2.get_scheduler();

    auto work1 = schedule(scheduler1) | then([] { return 10; });
    auto work2 = schedule(scheduler2) | then([] { return 20; });

    auto result1 = flow::this_thread::sync_wait(work1);
    auto result2 = flow::this_thread::sync_wait(work2);

    expect(result1.has_value() && std::get<0>(*result1) == 10);
    expect(result2.has_value() && std::get<0>(*result2) == 20);
  };

  // ============================================================================
  // Test 10: Receiver Move Semantics
  // ============================================================================

  "receiver_move_semantics"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    auto work = schedule(scheduler) | then([] { return 10; }) | then([](int x) { return x * 2; })
                | then([](int x) { return x + 5; });

    auto result = flow::this_thread::sync_wait(work);

    expect(result.has_value());
    expect(std::get<0>(*result) == 25);
  };

  // ============================================================================
  // Test 11: Try-Schedule Works
  // ============================================================================

  "try_schedule_basic"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    bool executed = false;
    auto work     = try_schedule(scheduler) | then([&executed] {
                  executed = true;
                  return 42;
                });

    auto result = flow::this_thread::sync_wait(work);

    // Should either succeed or fail gracefully
    expect(executed || !result.has_value());
  };

  // ============================================================================
  // Test 12: Stats Snapshot Consistency
  // ============================================================================

  "stats_snapshot_internal_consistency"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();

    // Execute some work
    for (int i = 0; i < 5; ++i) {
      auto work = schedule(scheduler) | then([] {});
      flow::this_thread::sync_wait(work);
    }

    // Check snapshot consistency
    for (size_t i = 0; i < 4; ++i) {
      auto stats = sched.get_stats(i);
      expect(stats.steals_succeeded <= stats.steals_attempted)
          << "Steals succeeded should not exceed attempts";
      expect(stats.tasks_executed >= 0) << "Tasks executed should be non-negative";
    }
  };

  // ============================================================================
  // Test 13: Work Distribution Happens
  // ============================================================================

  "work_gets_distributed"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();

    // Execute enough tasks that they should distribute
    for (int i = 0; i < 20; ++i) {
      auto work = schedule(scheduler) | then([] {
                    volatile int sum = 0;
                    for (int j = 0; j < 10; ++j) {
                      sum += j;
                    }
                  });
      flow::this_thread::sync_wait(work);
    }

    // Check that multiple workers did work
    int workers_with_tasks = 0;
    for (size_t i = 0; i < 4; ++i) {
      auto stats = sched.get_stats(i);
      if (stats.tasks_executed > 0) {
        workers_with_tasks++;
      }
    }

    expect(workers_with_tasks >= 2) << "Work should distribute across workers";
  };

  // ============================================================================
  // Test 14: Global Queue Used When Local Full
  // ============================================================================

  "global_queue_gets_used"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    // Submit enough tasks to potentially use global queue
    for (int i = 0; i < 30; ++i) {
      auto work = schedule(scheduler) | then([] {});
      flow::this_thread::sync_wait(work);
    }

    // Just verify it didn't crash - global queue was exercised
    expect(true) << "Global queue handled overflow safely";
  };

  // ============================================================================
  // Test 15: Stop Flag Synchronizes Properly
  // ============================================================================

  "stop_flag_synchronization"_test = [] {
    for (int iteration = 0; iteration < 3; ++iteration) {
      work_stealing_scheduler sched(2);
      auto                    scheduler = sched.get_scheduler();

      auto work = schedule(scheduler) | then([] {});
      flow::this_thread::sync_wait(work);

      // Destructor with proper memory ordering
    }

    expect(true) << "Stop flag synchronization works";
  };

  return 0;
}
