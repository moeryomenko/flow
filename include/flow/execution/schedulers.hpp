#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "completion_signatures.hpp"
#include "lock_free_queue.hpp"
#include "queries.hpp"
#include "scheduler.hpp"
#include "try_scheduler.hpp"
#include "type_list.hpp"

namespace flow::execution {

// [exec.sched.inline], inline scheduler
class inline_scheduler {
 public:
  using scheduler_concept = scheduler_t;

  inline_scheduler() = default;

  [[nodiscard]] static auto schedule() noexcept {
    return _schedule_sender{};
  }

  [[nodiscard]] static auto query(get_forward_progress_guarantee_t /*unused*/) noexcept {
    return forward_progress_guarantee::weakly_parallel;
  }

  auto operator==(const inline_scheduler&) const noexcept -> bool = default;

 private:
  struct _schedule_sender {
    using sender_concept = sender_t;
    using value_types    = type_list<>;  // schedule() sends no values

    template <class Env>
    auto get_completion_signatures(Env&& /*unused*/) const noexcept {
      return completion_signatures<set_value_t()>{};
    }

    template <receiver R>
    auto connect(R&& r) const noexcept {
      return _operation<R>{std::forward<R>(r)};
    }

    template <class Rcvr>
    struct _operation {
      using operation_state_concept = operation_state_t;

      Rcvr receiver_;

      void start() & noexcept {
        std::move(receiver_).set_value();
      }
    };
  };
};

// [exec.sched.run_loop], run_loop scheduler
class run_loop {
 public:
  run_loop() : stop_(false) {}

  ~run_loop() {
    finish();
  }

  run_loop(const run_loop&)                    = delete;
  auto operator=(const run_loop&) -> run_loop& = delete;

  class run_loop_scheduler {
   public:
    using scheduler_concept     = scheduler_t;
    using try_scheduler_concept = try_scheduler_t;

    explicit run_loop_scheduler(run_loop* loop) noexcept : loop_(loop) {}

    [[nodiscard]] auto schedule() const noexcept {
      return _schedule_sender{loop_};
    }

    [[nodiscard]] auto try_schedule() const noexcept {
      return _try_schedule_sender{loop_};
    }

    [[nodiscard]] static auto query(get_forward_progress_guarantee_t /*unused*/) noexcept {
      return forward_progress_guarantee::parallel;
    }

    auto operator==(const run_loop_scheduler& other) const noexcept -> bool {
      return loop_ == other.loop_;
    }

   private:
    run_loop* loop_;

    struct _schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;  // schedule() sends no values

      run_loop* loop_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_stopped_t()>{};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _operation<R>{loop_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _operation<R>{loop_, std::forward<R>(r)};
      }

      template <class Rcvr>
      struct _operation {
        using operation_state_concept = operation_state_t;

        run_loop* loop_;
        Rcvr      receiver_;

        void start() & noexcept {
          loop_->push_back([this] -> auto {
            if (!loop_->is_stopped()) {
              std::move(receiver_).set_value();
            } else {
              std::move(receiver_).set_stopped();
            }
          });
        }
      };
    };

    struct _try_schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;  // try_schedule() sends no values

      run_loop* loop_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_error_t(would_block_t), set_stopped_t()>{};
      }

      [[nodiscard]] auto query(get_completion_scheduler_t<set_value_t> /*unused*/) const noexcept {
        return run_loop_scheduler{loop_};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _try_operation<R>{loop_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _try_operation<R>{loop_, std::forward<R>(r)};
      }

      template <class Rcvr>
      struct _try_operation {
        using operation_state_concept = operation_state_t;

        run_loop* loop_;
        Rcvr      receiver_;

        void start() & noexcept {
          // Try non-blocking push
          if (loop_->try_push_back([this] -> auto {
                if (!loop_->is_stopped()) {
                  std::move(receiver_).set_value();
                } else {
                  std::move(receiver_).set_stopped();
                }
              })) {
            // Successfully enqueued, will complete later
          } else {
            // Would block - immediately signal error
            std::move(receiver_).set_error(would_block_t{});
          }
        }
      };
    };
  };

  auto get_scheduler() noexcept -> run_loop_scheduler {
    return run_loop_scheduler{this};
  }

  void run() {
    while (!stop_.load(std::memory_order_acquire)) {
      // First try lock-free queue (non-blocking)
      if (auto task = lock_free_queue_.try_pop()) {
        (*task)();
        continue;
      }

      // Then wait on regular queue
      std::unique_lock lock(mutex_);
      cv_.wait(lock,
               [this] -> bool { return !queue_.empty() || stop_.load(std::memory_order_relaxed); });

      if (stop_.load(std::memory_order_relaxed) && queue_.empty()) {
        // Check lock-free queue one more time before exiting
        if (auto task = lock_free_queue_.try_pop()) {
          lock.unlock();
          (*task)();
          continue;
        }
        break;
      }

      if (!queue_.empty()) {
        auto task = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        task();
      }
    }
  }

  void finish() {
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
  }

 private:
  friend class run_loop_scheduler;

  void push_back(std::function<void()> task) {
    {
      std::scoped_lock lock(mutex_);
      queue_.push(std::move(task));
    }
    cv_.notify_one();
  }

  // Non-blocking push for try_schedule
  auto try_push_back(std::function<void()> task) noexcept -> bool {
    // std::function move can potentially allocate, wrap in try-catch
    try {
      // Try to push to lock-free queue without blocking
      if (lock_free_queue_.try_push(std::move(task))) {
        cv_.notify_one();
        return true;
      }
      return false;  // Queue is full, would block
    } catch (...) {
      // Move construction threw (allocation failed)
      return false;
    }
  }

  auto is_stopped() const -> bool {
    return stop_.load(std::memory_order_acquire);
  }

  std::queue<std::function<void()>>                    queue_;
  lock_free_bounded_queue<std::function<void()>, 1024> lock_free_queue_;
  std::mutex                                           mutex_;
  std::condition_variable                              cv_;
  std::atomic<bool>                                    stop_;
};

// [exec.sched.thread_pool], thread_pool scheduler
class thread_pool {
 public:
  explicit thread_pool(std::size_t num_threads = std::thread::hardware_concurrency()) {
    workers_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this] -> void { worker_thread(); });
    }
  }

  ~thread_pool() {
    {
      std::scoped_lock lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  thread_pool(const thread_pool&)                    = delete;
  auto operator=(const thread_pool&) -> thread_pool& = delete;

  class thread_pool_scheduler {
   public:
    using scheduler_concept     = scheduler_t;
    using try_scheduler_concept = try_scheduler_t;

    explicit thread_pool_scheduler(thread_pool* pool) noexcept : pool_(pool) {}

    [[nodiscard]] auto schedule() const noexcept {
      return _schedule_sender{pool_};
    }

    [[nodiscard]] auto try_schedule() const noexcept {
      return _try_schedule_sender{pool_};
    }

    [[nodiscard]] static auto query(get_forward_progress_guarantee_t /*unused*/) noexcept {
      return forward_progress_guarantee::parallel;
    }

    auto operator==(const thread_pool_scheduler& other) const noexcept -> bool {
      return pool_ == other.pool_;
    }

   private:
    thread_pool* pool_;

    struct _schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;  // schedule() sends no values

      thread_pool* pool_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_error_t(std::exception_ptr)>{};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _operation<R>{pool_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _operation<R>{pool_, std::forward<R>(r)};
      }

      [[nodiscard]] auto query(get_completion_scheduler_t<set_value_t> /*unused*/) const noexcept {
        return thread_pool_scheduler{pool_};
      }

      template <class Rcvr>
      struct _operation {
        using operation_state_concept = operation_state_t;

        thread_pool*      pool_{};
        Rcvr              receiver_;
        std::atomic<bool> started_{false};

        void start() & noexcept {
          if (started_.exchange(true, std::memory_order_relaxed)) {
            return;
          }

          pool_->submit([this] -> auto {
            try {
              std::move(receiver_).set_value();
            } catch (...) {
              std::move(receiver_).set_error(std::current_exception());
            }
          });
        }
      };
    };

    struct _try_schedule_sender {
      using sender_concept = sender_t;
      using value_types    = type_list<>;  // try_schedule() sends no values

      thread_pool* pool_;

      template <class Env>
      auto get_completion_signatures(Env&& /*unused*/) const noexcept {
        return completion_signatures<set_value_t(), set_error_t(would_block_t),
                                     set_error_t(std::exception_ptr)>{};
      }

      template <receiver R>
      auto connect(R&& r) && {
        return _try_operation<R>{pool_, std::forward<R>(r)};
      }

      template <receiver R>
      auto connect(R&& r) & {
        return _try_operation<R>{pool_, std::forward<R>(r)};
      }

      [[nodiscard]] auto query(get_completion_scheduler_t<set_value_t> /*unused*/) const noexcept {
        return thread_pool_scheduler{pool_};
      }

      template <class Rcvr>
      struct _try_operation {
        using operation_state_concept = operation_state_t;

        thread_pool*      pool_{};
        Rcvr              receiver_;
        std::atomic<bool> started_{false};

        void start() & noexcept {
          if (started_.exchange(true, std::memory_order_relaxed)) {
            return;
          }

          bool submitted = pool_->try_submit([this] -> auto {
            try {
              std::move(receiver_).set_value();
            } catch (...) {
              std::move(receiver_).set_error(std::current_exception());
            }
          });

          if (submitted) {
            // Successfully enqueued, completion will happen asynchronously
          } else {
            // Would block - immediately signal error
            std::move(receiver_).set_error(would_block_t{});
          }
        }
      };
    };
  };

  auto get_scheduler() noexcept -> thread_pool_scheduler {
    return thread_pool_scheduler{this};
  }

 private:
  friend class thread_pool_scheduler;

  void submit(std::function<void()> task) {
    {
      std::scoped_lock lock(mutex_);
      if (stop_) {
        return;
      }
      queue_.push(std::move(task));
    }
    cv_.notify_one();
  }

  // Non-blocking submit for try_schedule
  auto try_submit(std::function<void()> task) noexcept -> bool {
    // std::function move can potentially allocate, wrap in try-catch
    try {
      // Try to push to lock-free queue without blocking
      if (lock_free_queue_.try_push(std::move(task))) {
        lock_free_has_work_.store(true, std::memory_order_release);
        cv_.notify_one();
        return true;
      }
      return false;  // Queue is full, would block
    } catch (...) {
      // Move construction threw (allocation failed)
      return false;
    }
  }

  void worker_thread() {
    while (true) {
      // First try lock-free queue (non-blocking)
      if (auto task = lock_free_queue_.try_pop()) {
        lock_free_has_work_.store(false, std::memory_order_relaxed);
        (*task)();
        continue;
      }

      std::function<void()> task;

      {
        std::unique_lock lock(mutex_);

        // Wait if both queues appear empty
        cv_.wait(lock, [this] -> bool {
          // Wake up if stop flag is set, regular queue has items, OR lock-free queue might have
          // work
          return stop_ || !queue_.empty() || lock_free_has_work_.load(std::memory_order_acquire);
        });

        if (stop_ && queue_.empty()) {
          // Check lock-free queue one more time before exiting
          if (auto final_task = lock_free_queue_.try_pop()) {
            (*final_task)();
          }
          return;
        }

        if (!queue_.empty()) {
          task = std::move(queue_.front());
          queue_.pop();
        }
      }

      if (task) {
        task();
      }
      // If no task from regular queue, loop back to check lock-free queue
    }
  }

  lock_free_bounded_queue<std::function<void()>, 1024> lock_free_queue_;
  std::vector<std::thread>                             workers_;
  std::queue<std::function<void()>>                    queue_;
  std::condition_variable                              cv_;
  std::mutex                                           mutex_;
  std::atomic<bool>                                    lock_free_has_work_{false};
  bool                                                 stop_{false};
};

}  // namespace flow::execution
