#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "completion_signatures.hpp"
#include "queries.hpp"
#include "scheduler.hpp"
#include "try_scheduler.hpp"
#include "type_list.hpp"

namespace flow::execution {

// Work-stealing scheduler inspired by Go's runtime :
//
// 1. Per-processor local run queues (like Go's P)
// 2. Work-stealing from other processors
// 3. Global run queue for overflow and load balancing
//
// Architecture:
// - G (goroutine): Lightweight task abstraction
// - P (processor): Logical processor with local run queue
// - M (machine): OS thread that executes tasks from P

class work_stealing_scheduler {
 public:
  // Task represents a unit of work (analogous to Go's G)
  struct task {
    std::function<void()> work;
    std::atomic<uint64_t> sequence{0};  // For ordering and fairness
    std::atomic<bool>     cancelled{false};

    explicit task(std::function<void()> w) : work(std::move(w)) {}
  };

  // Processor context (analogous to Go's P)
  // Each P has a local run queue to minimize contention
  class processor_context {
   public:
    processor_context() : rng_(std::random_device{}()) {}

    // Try to push task to local queue (returns false if full)
    auto try_push_local(std::shared_ptr<task> t) -> bool {
      std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
      if (!lock.owns_lock()) {
        return false;
      }

      // Local queue size limit (like Go's 256)
      constexpr size_t local_queue_max = 256;
      if (local_queue_.size() >= local_queue_max) {
        return false;
      }

      // Use release ordering to ensure task is fully constructed before being visible
      t->sequence.store(next_sequence_++, std::memory_order_release);
      local_queue_.push_back(std::move(t));
      return true;
    }

    // Pop from front of local queue (FIFO for cache locality)
    auto pop_local() -> std::shared_ptr<task> {
      std::scoped_lock lock(mutex_);
      if (local_queue_.empty()) {
        return nullptr;
      }

      auto t = std::move(local_queue_.front());
      local_queue_.pop_front();
      return t;
    }

    // Steal from back of queue (LIFO to reduce contention with owner)
    auto try_steal() -> std::shared_ptr<task> {
      std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
      if (!lock.owns_lock() || local_queue_.empty()) {
        return nullptr;
      }

      // Steal from back (oldest task for better fairness)
      auto t = std::move(local_queue_.back());
      local_queue_.pop_back();
      return t;
    }

    // Check if queue has work
    auto has_work() const -> bool {
      std::scoped_lock lock(mutex_);
      return !local_queue_.empty();
    }

    // Get approximate queue size (for load balancing)
    auto queue_size() const -> size_t {
      std::scoped_lock lock(mutex_);
      return local_queue_.size();
    }

    // Generate random processor id for work stealing
    auto random_victim(size_t num_procs, size_t self_id) -> size_t {
      std::scoped_lock                      lock(rng_mutex_);
      std::uniform_int_distribution<size_t> dist(0, num_procs - 1);
      size_t                                victim = dist(rng_);
      // Ensure we don't steal from ourselves
      if (victim == self_id && num_procs > 1) {
        victim = (victim + 1) % num_procs;
      }
      return victim;
    }

   private:
    mutable std::mutex                mutex_;
    std::deque<std::shared_ptr<task>> local_queue_;
    uint64_t                          next_sequence_{0};

    // RNG for work stealing victim selection
    mutable std::mutex rng_mutex_;
    std::mt19937       rng_;
  };

  // Global run queue for overflow and load balancing
  class global_queue {
   public:
    void push(std::shared_ptr<task> t) {
      std::scoped_lock lock(mutex_);
      queue_.push_back(std::move(t));
      has_work_.store(true, std::memory_order_release);
    }

    auto try_pop() -> std::shared_ptr<task> {
      std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
      if (!lock.owns_lock() || queue_.empty()) {
        return nullptr;
      }

      auto t = std::move(queue_.front());
      queue_.pop_front();

      if (queue_.empty()) {
        has_work_.store(false, std::memory_order_release);
      }

      return t;
    }

    auto has_work() const -> bool {
      return has_work_.load(std::memory_order_acquire);
    }

    auto size() const -> size_t {
      std::scoped_lock lock(mutex_);
      return queue_.size();
    }

   private:
    mutable std::mutex                mutex_;
    std::deque<std::shared_ptr<task>> queue_;
    std::atomic<bool>                 has_work_{false};
  };

  explicit work_stealing_scheduler(std::size_t num_threads = std::thread::hardware_concurrency())
      : num_procs_(num_threads), stop_(false) {
    if (num_threads == 0) {
      throw std::invalid_argument("Number of threads must be greater than 0");
    }

    // Initialize processor contexts
    procs_.reserve(num_procs_);
    for (size_t i = 0; i < num_procs_; ++i) {
      procs_.emplace_back(std::make_unique<processor_context>());
    }

    // Initialize dynamic stats array
    worker_stats_.resize(num_procs_);

    // Launch worker threads (M in Go terminology)
    workers_.reserve(num_procs_);
    for (size_t i = 0; i < num_procs_; ++i) {
      workers_.emplace_back([this, proc_id = i] { worker_thread(proc_id); });
    }
  }

  ~work_stealing_scheduler() {
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();

    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  work_stealing_scheduler(const work_stealing_scheduler&)                    = delete;
  auto operator=(const work_stealing_scheduler&) -> work_stealing_scheduler& = delete;
  work_stealing_scheduler(work_stealing_scheduler&&)                         = delete;
  auto operator=(work_stealing_scheduler&&) -> work_stealing_scheduler&      = delete;

  class work_stealing_scheduler_handle {
   public:
    using scheduler_concept     = scheduler_t;
    using try_scheduler_concept = try_scheduler_t;

    explicit work_stealing_scheduler_handle(work_stealing_scheduler* sched) noexcept
        : sched_(sched) {}

    [[nodiscard]] auto schedule() const noexcept {
      return _schedule_sender{sched_};
    }

    [[nodiscard]] auto try_schedule() const noexcept {
      return _try_schedule_sender{sched_};
    }

    [[nodiscard]] static auto query(get_forward_progress_guarantee_t /*unused*/) noexcept {
      return forward_progress_guarantee::parallel;
    }

    auto operator==(const work_stealing_scheduler_handle& other) const noexcept -> bool {
      return sched_ == other.sched_;
    }

   private:
    work_stealing_scheduler* sched_;

    struct _schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;

      work_stealing_scheduler* sched_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_error_t(std::exception_ptr)>{};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _operation<std::remove_cvref_t<R>>{sched_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _operation<std::remove_cvref_t<R>>{sched_, std::forward<R>(r)};
      }

      [[nodiscard]] auto query(get_completion_scheduler_t<set_value_t> /*unused*/) const noexcept {
        return work_stealing_scheduler_handle{sched_};
      }

      template <class Rcvr>
      struct _operation {
        using operation_state_concept = operation_state_t;

        work_stealing_scheduler* sched_;
        Rcvr                     receiver_;

        void start() & noexcept {
          // SAFETY: The scheduler must outlive all operations.
          // Users must ensure scheduler lifetime exceeds operations.
          try {
            sched_->submit([rcvr = std::move(receiver_)]() mutable {
              try {
                std::move(rcvr).set_value();
              } catch (...) {
                std::move(rcvr).set_error(std::current_exception());
              }
            });
          } catch (...) {
            // If submit throws, call set_error on the receiver
            std::move(receiver_).set_error(std::current_exception());
          }
        }
      };
    };

    struct _try_schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;

      work_stealing_scheduler* sched_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_error_t(would_block_t),
                                     set_error_t(std::exception_ptr)>{};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _try_operation<std::remove_cvref_t<R>>{sched_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _try_operation<std::remove_cvref_t<R>>{sched_, std::forward<R>(r)};
      }

      [[nodiscard]] auto query(get_completion_scheduler_t<set_value_t> /*unused*/) const noexcept {
        return work_stealing_scheduler_handle{sched_};
      }

      template <class Rcvr>
      struct _try_operation {
        using operation_state_concept = operation_state_t;

        work_stealing_scheduler* sched_;
        Rcvr                     receiver_;

        void start() & noexcept {
          try {
            bool submitted = sched_->try_submit([rcvr = std::move(receiver_)]() mutable {
              try {
                std::move(rcvr).set_value();
              } catch (...) {
                std::move(rcvr).set_error(std::current_exception());
              }
            });

            if (!submitted) {
              std::move(receiver_).set_error(would_block_t{});
            }
          } catch (...) {
            std::move(receiver_).set_error(std::current_exception());
          }
        }
      };
    };
  };

  auto get_scheduler() noexcept -> work_stealing_scheduler_handle {
    return work_stealing_scheduler_handle{this};
  }

  // Statistics for monitoring and debugging
  struct stats {
    std::atomic<uint64_t> tasks_executed{0};
    std::atomic<uint64_t> steals_attempted{0};
    std::atomic<uint64_t> steals_succeeded{0};
    std::atomic<uint64_t> global_queue_pops{0};
    std::atomic<uint64_t> local_queue_pops{0};

    // Make movable for vector operations
    stats() = default;
    stats(const stats& other)
        : tasks_executed(other.tasks_executed.load(std::memory_order_relaxed)),
          steals_attempted(other.steals_attempted.load(std::memory_order_relaxed)),
          steals_succeeded(other.steals_succeeded.load(std::memory_order_relaxed)),
          global_queue_pops(other.global_queue_pops.load(std::memory_order_relaxed)),
          local_queue_pops(other.local_queue_pops.load(std::memory_order_relaxed)) {}

    stats(stats&& other) noexcept
        : tasks_executed(other.tasks_executed.load(std::memory_order_relaxed)),
          steals_attempted(other.steals_attempted.load(std::memory_order_relaxed)),
          steals_succeeded(other.steals_succeeded.load(std::memory_order_relaxed)),
          global_queue_pops(other.global_queue_pops.load(std::memory_order_relaxed)),
          local_queue_pops(other.local_queue_pops.load(std::memory_order_relaxed)) {}

    stats& operator=(const stats& other) {
      tasks_executed.store(other.tasks_executed.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
      steals_attempted.store(other.steals_attempted.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      steals_succeeded.store(other.steals_succeeded.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      global_queue_pops.store(other.global_queue_pops.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      local_queue_pops.store(other.local_queue_pops.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      return *this;
    }

    stats& operator=(stats&& other) noexcept {
      tasks_executed.store(other.tasks_executed.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
      steals_attempted.store(other.steals_attempted.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      steals_succeeded.store(other.steals_succeeded.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      global_queue_pops.store(other.global_queue_pops.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
      local_queue_pops.store(other.local_queue_pops.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
      return *this;
    }
  };

  // Return a snapshot of stats for a processor
  struct stats_snapshot {
    uint64_t tasks_executed;
    uint64_t steals_attempted;
    uint64_t steals_succeeded;
    uint64_t global_queue_pops;
    uint64_t local_queue_pops;
  };

  auto get_stats(size_t proc_id) const -> stats_snapshot {
    if (proc_id >= num_procs_) {
      return {.tasks_executed    = 0,
              .steals_attempted  = 0,
              .steals_succeeded  = 0,
              .global_queue_pops = 0,
              .local_queue_pops  = 0};
    }
    const auto& s = worker_stats_[proc_id];
    return {.tasks_executed    = s.tasks_executed.load(std::memory_order_relaxed),
            .steals_attempted  = s.steals_attempted.load(std::memory_order_relaxed),
            .steals_succeeded  = s.steals_succeeded.load(std::memory_order_relaxed),
            .global_queue_pops = s.global_queue_pops.load(std::memory_order_relaxed),
            .local_queue_pops  = s.local_queue_pops.load(std::memory_order_relaxed)};
  }

 private:
  void submit(std::function<void()> work) {
    auto t = std::make_shared<task>(std::move(work));

    // Try to submit to a random processor's local queue
    // Use thread-local RNG for better performance (avoids repeated random_device construction)
    thread_local std::mt19937             rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, num_procs_ - 1);
    size_t                                proc_id = dist(rng);

    if (!procs_[proc_id]->try_push_local(t)) {
      // Local queue full, use global queue
      global_queue_.push(std::move(t));
    }

    // Wake up a worker
    cv_.notify_one();
  }

  auto try_submit(std::function<void()> work) noexcept -> bool {
    try {
      auto t = std::make_shared<task>(std::move(work));

      // Try round-robin placement to balance load
      size_t start_proc = next_proc_.fetch_add(1, std::memory_order_relaxed) % num_procs_;

      // Try several processors before giving up
      for (size_t i = 0; i < num_procs_; ++i) {
        size_t proc_id = (start_proc + i) % num_procs_;
        if (procs_[proc_id]->try_push_local(t)) {
          cv_.notify_one();
          return true;
        }
      }

      // All local queues full, would need to use global queue which might block
      return false;
    } catch (...) {
      return false;
    }
  }

  void worker_thread(size_t proc_id) {
    auto& proc  = procs_[proc_id];
    auto& stats = worker_stats_[proc_id];

    constexpr size_t work_batch_size = 32;  // Process up to 32 tasks before checking

    while (!stop_.load(std::memory_order_acquire)) {
      size_t processed = 0;

      // Phase 1: Process local queue (best cache locality)
      while (processed < work_batch_size) {
        auto t = proc->pop_local();
        if (!t) {
          break;
        }

        if (!t->cancelled.load(std::memory_order_acquire)) {
          t->work();
          stats.tasks_executed.fetch_add(1, std::memory_order_relaxed);
          stats.local_queue_pops.fetch_add(1, std::memory_order_relaxed);
        }
        processed++;
      }

      // Phase 2: Check global queue periodically (1 in 61 like Go)
      // This provides fairness and prevents global queue starvation
      if (stats.tasks_executed.load(std::memory_order_relaxed) % 61 == 0
          && global_queue_.has_work()) {
        if (auto t = global_queue_.try_pop()) {
          if (!t->cancelled.load(std::memory_order_acquire)) {
            t->work();
            stats.tasks_executed.fetch_add(1, std::memory_order_relaxed);
            stats.global_queue_pops.fetch_add(1, std::memory_order_relaxed);
          }
          processed++;
        }
      }

      // Phase 3: Work stealing (only if we have no local work)
      if (processed == 0 && num_procs_ > 1) {
        // Try stealing multiple times before giving up
        constexpr size_t steal_attempts = 4;
        for (size_t attempt = 0; attempt < steal_attempts; ++attempt) {
          stats.steals_attempted.fetch_add(1, std::memory_order_relaxed);

          size_t victim_id = proc->random_victim(num_procs_, proc_id);
          auto   stolen    = procs_[victim_id]->try_steal();

          if (stolen) {
            stats.steals_succeeded.fetch_add(1, std::memory_order_relaxed);
            if (!stolen->cancelled.load(std::memory_order_acquire)) {
              stolen->work();
              stats.tasks_executed.fetch_add(1, std::memory_order_relaxed);
            }
            processed++;
            break;  // Successfully stole and executed
          }
        }
      }

      // Phase 4: If no work found, wait
      if (processed == 0) {
        std::unique_lock<std::mutex> lock(cv_mutex_);

        // Double-check before waiting (avoid missed wakeup)
        // Use acquire ordering to synchronize with submit/try_submit
        bool has_work = proc->has_work() || global_queue_.has_work() || any_proc_has_work(proc_id);

        if (!has_work && !stop_.load(std::memory_order_acquire)) {
          // Wait for work or shutdown
          // Use timed wait to periodically check for work stealing opportunities
          cv_.wait_for(lock, std::chrono::microseconds(100), [this, &proc, proc_id] {
            return stop_.load(std::memory_order_acquire) || proc->has_work()
                   || global_queue_.has_work() || any_proc_has_work(proc_id);
          });
        }
      }
      // If we processed work, immediately check for more (stay hot)
    }

    // Cleanup: process remaining local work before exiting
    while (auto t = proc->pop_local()) {
      if (!t->cancelled.load(std::memory_order_acquire)) {
        t->work();
        stats.tasks_executed.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  // Check if any processor has work (for work stealing decision)
  auto any_proc_has_work(size_t exclude_proc) const -> bool {
    for (size_t i = 0; i < num_procs_; ++i) {
      if (i != exclude_proc && procs_[i]->has_work()) {
        return true;
      }
    }
    return false;
  }

  const size_t                                    num_procs_;
  std::vector<std::unique_ptr<processor_context>> procs_;
  global_queue                                    global_queue_;
  std::vector<std::thread>                        workers_;
  std::condition_variable                         cv_;
  mutable std::mutex                              cv_mutex_;
  std::atomic<bool>                               stop_;
  std::atomic<size_t>                             next_proc_{0};

  // Per-worker statistics (dynamic sizing to handle any thread count)
  std::vector<stats> worker_stats_;
};

}  // namespace flow::execution
