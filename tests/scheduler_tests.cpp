#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <thread>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "inline_scheduler_basic"_test = [] {
    inline_scheduler sch;
    bool             executed = false;

    auto                  work   = schedule(sch) | then([&] { executed = true; });
    [[maybe_unused]] auto result = flow::this_thread::sync_wait(work);

    expect(executed);
  };

  "inline_scheduler_same_thread"_test = [] {
    inline_scheduler sch;
    std::thread::id  main_thread = std::this_thread::get_id();
    std::thread::id  work_thread;

    auto work = schedule(sch) | then([&] { work_thread = std::this_thread::get_id(); });

    flow::this_thread::sync_wait(work);

    expect(work_thread == main_thread);
  };

  "inline_scheduler_forward_progress"_test = [] {
    [[maybe_unused]] inline_scheduler sch;
    auto fpg = flow::execution::inline_scheduler::query(get_forward_progress_guarantee);

    expect(fpg == forward_progress_guarantee::weakly_parallel);
  };

  // Thread pool tests would go here if thread_pool is implemented
  // "thread_pool_scheduler_basic"_test = [] { ... };

  // Run loop tests would go here if run_loop is implemented
  // "run_loop_scheduler_basic"_test = [] { ... };

  // Concurrent scheduler tests (P3669) would go here
  // "concurrent_scheduler_try_start"_test = [] { ... };
}
