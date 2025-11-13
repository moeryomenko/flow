// Example demonstrating work-stealing scheduler with EEVDF-aware design
//
// This example shows:
// 1. Basic usage of work-stealing scheduler
// 2. Performance comparison with thread_pool
// 3. Work-stealing statistics and monitoring
// 4. Integration with flow's sender/receiver model

#include <atomic>
#include <chrono>
#include <cmath>
#include <flow/execution.hpp>
#include <format>
#include <iostream>
#include <numeric>
#include <vector>

using namespace flow::execution;

// Simulate CPU-bound work with varying computational intensity
auto compute_intensive_task(int id, int iterations) -> double {
  double result = 0.0;
  for (int i = 0; i < iterations; ++i) {
    result += std::sin(i * 0.001) * std::cos(id * 0.001);
  }
  return result;
}

// Example 1: Basic work-stealing scheduler usage
void example_basic_usage() {
  std::cout << "\n=== Example 1: Basic Usage ===\n";

  // Create work-stealing scheduler with hardware concurrency
  work_stealing_scheduler sched;
  auto                    scheduler = sched.get_scheduler();

  // Submit multiple tasks
  constexpr int       num_tasks = 100;
  std::vector<double> results(num_tasks);

  auto start = std::chrono::steady_clock::now();

  // Execute tasks using sync_wait
  for (int i = 0; i < num_tasks; ++i) {
    auto work = schedule(scheduler) | then([i] { return compute_intensive_task(i, 10000); });

    auto result = flow::this_thread::sync_wait(work);
    if (result) {
      results[i] = std::get<0>(*result);
    }
  }

  auto end      = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << std::format("Completed {} tasks in {}ms\n", num_tasks, duration.count());
  std::cout << std::format("Average per task: {:.2f}ms\n",
                           static_cast<double>(duration.count()) / num_tasks);
}

// Example 2: Work-stealing statistics
void example_statistics() {
  std::cout << "\n=== Example 2: Work-Stealing Statistics ===\n";

  work_stealing_scheduler sched(4);  // Use 4 workers for clearer stats
  auto                    scheduler = sched.get_scheduler();

  constexpr int num_tasks = 1000;

  // Submit tasks with varying computational intensity
  for (int i = 0; i < num_tasks; ++i) {
    auto work = schedule(scheduler) | then([i] {
                  // Vary task duration to trigger work stealing
                  int iterations = (i % 10 == 0) ? 50000 : 5000;
                  compute_intensive_task(i, iterations);
                });

    flow::this_thread::sync_wait(work);
  }

  // Print statistics for each processor
  std::cout << "\nPer-Processor Statistics:\n";
  std::cout << "---------------------------------------------\n";
  std::cout << "P#  Tasks  Local  Global  Steals  Success%\n";
  std::cout << "---------------------------------------------\n";

  uint64_t total_tasks   = 0;
  uint64_t total_steals  = 0;
  uint64_t total_success = 0;

  for (size_t i = 0; i < 4; ++i) {
    auto stats = sched.get_stats(i);

    double success_rate = stats.steals_attempted > 0
                              ? 100.0 * static_cast<double>(stats.steals_succeeded)
                                    / static_cast<double>(stats.steals_attempted)
                              : 0.0;

    std::cout << std::format("{:<3} {:<6} {:<6} {:<7} {:<7} {:>6.1f}%\n", i, stats.tasks_executed,
                             stats.local_queue_pops, stats.global_queue_pops,
                             stats.steals_succeeded, success_rate);

    total_tasks += stats.tasks_executed;
    total_steals += stats.steals_attempted;
    total_success += stats.steals_succeeded;
  }

  std::cout << "---------------------------------------------\n";
  std::cout << std::format("Total tasks executed: {}\n", total_tasks);
  std::cout << std::format("Total steal attempts: {}\n", total_steals);
  std::cout << std::format("Overall steal success: {:.1f}%\n",
                           total_steals > 0 ? 100.0 * static_cast<double>(total_success)
                                                  / static_cast<double>(total_steals)
                                            : 0.0);
}

// Example 3: Comparison with standard thread_pool
void example_performance_comparison() {
  std::cout << "\n=== Example 3: Performance Comparison ===\n";

  constexpr int num_tasks      = 500;
  constexpr int num_iterations = 20000;

  // Test work-stealing scheduler
  {
    work_stealing_scheduler sched;
    auto                    scheduler = sched.get_scheduler();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
      auto work = schedule(scheduler) | then([i] { compute_intensive_task(i, num_iterations); });

      flow::this_thread::sync_wait(work);
    }

    auto end      = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::format("Work-Stealing Scheduler: {}ms\n", duration.count());
  }

  // Test standard thread_pool
  {
    thread_pool pool;
    auto        scheduler = pool.get_scheduler();

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
      auto work = schedule(scheduler) | then([i] { compute_intensive_task(i, num_iterations); });

      flow::this_thread::sync_wait(work);
    }

    auto end      = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::format("Thread Pool Scheduler:  {}ms\n", duration.count());
  }
}

// Example 4: EEVDF-aware task distribution
void example_eevdf_awareness() {
  std::cout << "\n=== Example 4: EEVDF-Aware Task Distribution ===\n";
  std::cout << "Work-stealing scheduler benefits from EEVDF:\n";
  std::cout << "- Short time slices align with EEVDF base slice (~700μs)\n";
  std::cout << "- Minimal lock contention avoids priority inversion\n";
  std::cout << "- Local queues reduce cache misses and improve throughput\n";
  std::cout << "- Work stealing provides natural load balancing\n\n";

  work_stealing_scheduler sched(8);  // Use 8 workers
  auto                    scheduler = sched.get_scheduler();

  // Create workload with heterogeneous task durations
  std::vector<int> task_durations = {1000, 5000, 10000, 2000, 50000, 1000, 3000, 100000};

  auto start = std::chrono::steady_clock::now();

  // Submit tasks multiple times to see load balancing
  for (int round = 0; round < 10; ++round) {
    for (size_t i = 0; i < task_durations.size(); ++i) {
      auto work = schedule(scheduler) | then([i, duration = task_durations[i]] {
                    compute_intensive_task(static_cast<int>(i), duration);
                  });

      flow::this_thread::sync_wait(work);
    }
  }

  int total_tasks = static_cast<int>(task_durations.size()) * 10;

  auto end      = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << std::format("Completed {} heterogeneous tasks in {}ms\n", total_tasks,
                           duration.count());

  // Show load distribution
  std::cout << "\nLoad Distribution (tasks per processor):\n";
  std::vector<uint64_t> task_counts;
  for (size_t i = 0; i < 8; ++i) {
    auto stats = sched.get_stats(i);
    task_counts.push_back(stats.tasks_executed);
    std::cout << std::format("P{}: {} tasks\n", i, stats.tasks_executed);
  }

  // Calculate load balance metric (standard deviation)
  double mean = std::accumulate(task_counts.begin(), task_counts.end(), 0.0)
                / static_cast<double>(task_counts.size());
  double sq_sum =
      std::inner_product(task_counts.begin(), task_counts.end(), task_counts.begin(), 0.0);
  double stdev = std::sqrt((sq_sum / static_cast<double>(task_counts.size())) - (mean * mean));

  std::cout << std::format("\nLoad balance (lower is better): {:.2f}\n", stdev / mean);
}

auto main() -> int {
  try {
    std::cout << "Work-Stealing Scheduler Examples\n";
    std::cout << "==================================\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

    example_basic_usage();
    example_statistics();
    example_performance_comparison();
    example_eevdf_awareness();

    std::cout << "\n=== All examples completed successfully ===\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
