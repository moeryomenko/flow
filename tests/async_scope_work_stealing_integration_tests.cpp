#include <atomic>
#include <boost/ut.hpp>
#include <flow/execution.hpp>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  // ============================================================================
  // Test 1: Basic associate with work_stealing_scheduler
  // ============================================================================

  "associate_basic"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;
    auto                    token = scope.get_token();

    std::atomic<int> count{0};

    // Associate and execute tasks
    for (int i = 0; i < 5; ++i) {
      auto work = associate(
          schedule(scheduler) | then([&count] { count.fetch_add(1, std::memory_order_relaxed); }),
          token);
      flow::this_thread::sync_wait(work);
    }

    scope.close();
    auto join_result = flow::this_thread::sync_wait(scope.join());

    expect(count.load() == 5) << "All tasks should complete";
    expect(join_result.has_value()) << "join should succeed";
  };

  // ============================================================================
  // Test 2: Multiple scopes with same scheduler
  // ============================================================================

  "multiple_scopes"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    counting_scope scope1;
    counting_scope scope2;

    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    // Scope 1 work
    for (int i = 0; i < 3; ++i) {
      auto work = associate(
          schedule(scheduler) | then([&count1] { count1.fetch_add(1, std::memory_order_relaxed); }),
          scope1.get_token());
      flow::this_thread::sync_wait(work);
    }

    // Scope 2 work
    for (int i = 0; i < 4; ++i) {
      auto work = associate(
          schedule(scheduler) | then([&count2] { count2.fetch_add(1, std::memory_order_relaxed); }),
          scope2.get_token());
      flow::this_thread::sync_wait(work);
    }

    scope1.close();
    scope2.close();
    flow::this_thread::sync_wait(scope1.join());
    flow::this_thread::sync_wait(scope2.join());

    expect(count1.load() == 3) << "Scope1 tasks complete";
    expect(count2.load() == 4) << "Scope2 tasks complete";
  };

  // ============================================================================
  // Test 3: Work distribution
  // ============================================================================

  "work_distribution"_test = [] {
    work_stealing_scheduler sched(4);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;

    std::atomic<int> total{0};

    for (int i = 0; i < 20; ++i) {
      auto work = associate(schedule(scheduler) | then([&total, i] {
                              // Variable work
                              volatile int sum = 0;
                              for (int j = 0; j < (i % 5) * 10; ++j) {
                                sum += j;
                              }
                              total.fetch_add(1, std::memory_order_relaxed);
                            }),
                            scope.get_token());
      flow::this_thread::sync_wait(work);
    }

    scope.close();
    flow::this_thread::sync_wait(scope.join());

    expect(total.load() == 20) << "All tasks complete";

    // Check distribution
    int active_workers = 0;
    for (size_t i = 0; i < 4; ++i) {
      if (sched.get_stats(i).tasks_executed > 0) {
        active_workers++;
      }
    }

    expect(active_workers >= 1) << "At least one worker processed tasks";
  };

  // ============================================================================
  // Test 4: Exception handling
  // ============================================================================

  "exception_handling"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;

    std::atomic<int> successful{0};

    for (int i = 0; i < 6; ++i) {
      try {
        auto work = associate(schedule(scheduler) | then([&successful, i] {
                                if (i % 2 == 0) {
                                  throw std::runtime_error("test");
                                }
                                successful.fetch_add(1, std::memory_order_relaxed);
                              }),
                              scope.get_token());
        flow::this_thread::sync_wait(work);
      } catch (const std::exception&) {
        // Expected
      }
    }

    scope.close();
    flow::this_thread::sync_wait(scope.join());

    expect(successful.load() == 3) << "Non-throwing tasks complete";
  };

  // ============================================================================
  // Test 5: Stop token integration
  // ============================================================================

  "stop_token"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;

    for (int i = 0; i < 3; ++i) {
      auto work = associate(schedule(scheduler) | then([] {}), scope.get_token());
      flow::this_thread::sync_wait(work);
    }

    auto stop_token = scope.get_stop_token();
    expect(!stop_token.stop_requested()) << "Not stopped initially";

    scope.request_stop();
    expect(stop_token.stop_requested()) << "Stopped after request";

    scope.close();
    flow::this_thread::sync_wait(scope.join());
  };

  // ============================================================================
  // Test 6: Chained operations
  // ============================================================================

  "chained_ops"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;

    std::atomic<int> sum{0};

    for (int i = 0; i < 5; ++i) {
      auto work = associate(
          schedule(scheduler) | then([i] { return i * 2; }) | then([](int x) { return x + 10; })
              | then([&sum](int x) { sum.fetch_add(x, std::memory_order_relaxed); }),
          scope.get_token());
      flow::this_thread::sync_wait(work);
    }

    scope.close();
    flow::this_thread::sync_wait(scope.join());

    // (0*2)+10=10, (1*2)+10=12, (2*2)+10=14, (3*2)+10=16, (4*2)+10=18 => 70
    expect(sum.load() == 70) << "Chained ops correct";
  };

  // ============================================================================
  // Test 7: simple_counting_scope integration
  // ============================================================================

  "simple_scope"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    simple_counting_scope   scope;

    std::atomic<int> sum{0};

    for (int i = 0; i < 5; ++i) {
      auto work = associate(
          schedule(scheduler) | then([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); }),
          scope.get_token());
      flow::this_thread::sync_wait(work);
    }

    scope.close();
    flow::this_thread::sync_wait(scope.join());

    // 0+1+2+3+4 = 10
    expect(sum.load() == 10) << "simple_counting_scope works";
  };

  // ============================================================================
  // Test 8: Scheduler statistics tracking
  // ============================================================================

  "scheduler_stats"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();
    counting_scope          scope;

    for (int i = 0; i < 10; ++i) {
      auto work = associate(schedule(scheduler) | then([] {
                              volatile int x = 0;
                              for (int j = 0; j < 50; ++j) {
                                x += j;
                              }
                            }),
                            scope.get_token());
      flow::this_thread::sync_wait(work);
    }

    scope.close();
    flow::this_thread::sync_wait(scope.join());

    uint64_t total = 0;
    for (size_t i = 0; i < 2; ++i) {
      total += sched.get_stats(i).tasks_executed;
    }

    expect(total >= 10) << "Stats track tasks";
  };

  // ============================================================================
  // Test 9: Scope lifecycle
  // ============================================================================

  "lifecycle"_test = [] {
    work_stealing_scheduler sched(2);
    auto                    scheduler = sched.get_scheduler();

    std::atomic<int> completed{0};

    {
      counting_scope scope;

      for (int i = 0; i < 3; ++i) {
        auto work = associate(schedule(scheduler) | then([&completed] {
                                completed.fetch_add(1, std::memory_order_relaxed);
                              }),
                              scope.get_token());
        flow::this_thread::sync_wait(work);
      }

      scope.close();
      flow::this_thread::sync_wait(scope.join());
    }

    expect(completed.load() == 3) << "Work completes before destruction";
  };

  // ============================================================================
  // Test 10: Token association before close
  // ============================================================================

  "token_association"_test = [] {
    work_stealing_scheduler sched(2);
    counting_scope          scope;
    auto                    token = scope.get_token();

    expect(token.try_associate()) << "Token associates";
    token.disassociate();

    scope.close();

    expect(!token.try_associate()) << "Token doesn't associate after close";
  };

  return 0;
}
