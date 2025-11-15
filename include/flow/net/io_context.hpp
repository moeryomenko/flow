#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>

#include "../execution/scheduler.hpp"
#include "concepts.hpp"

namespace flow::net {

// [net.io_context] I/O execution context
// Based on P2762R2 with P2300 sender/receiver integration
//
// This is a simplified I/O context that provides:
// 1. A scheduler for network operations
// 2. Event loop for processing I/O completions
// 3. Integration with system I/O facilities (poll/epoll/kqueue/io_uring)
//
// Note: This is a basic implementation. A production implementation would
// use platform-specific I/O facilities (io_uring on Linux, kqueue on BSD/macOS,
// IOCP on Windows, etc.)

class io_context {
 public:
  class scheduler_type;

  io_context() : context_id_(reinterpret_cast<std::uintptr_t>(this)), stopped_(false) {}

  io_context(io_context const&)            = delete;
  io_context& operator=(io_context const&) = delete;

  ~io_context() {
    stop();
  }

  // Get scheduler for this context
  [[nodiscard]] scheduler_type get_scheduler() noexcept;

  // Get unique context identifier
  [[nodiscard]] std::uintptr_t context_id() const noexcept {
    return context_id_;
  }

  // Run the event loop
  // Processes pending I/O operations until stopped
  std::size_t run() {
    std::size_t count = 0;
    while (!stopped_.load(std::memory_order_acquire)) {
      count += run_one();
      if (count == 0 && work_queue_.empty()) {
        break;
      }
    }
    return count;
  }

  // Run at most one pending operation
  std::size_t run_one() {
    std::function<void()> work;
    {
      std::unique_lock lock(mutex_);
      if (work_queue_.empty()) {
        return 0;
      }
      work = std::move(work_queue_.front());
      work_queue_.pop();
    }

    if (work) {
      work();
      return 1;
    }
    return 0;
  }

  // Stop the event loop
  void stop() {
    stopped_.store(true, std::memory_order_release);
    cv_.notify_all();
  }

  // Reset the stopped state
  void restart() {
    stopped_.store(false, std::memory_order_release);
  }

  // Check if stopped
  [[nodiscard]] bool stopped() const noexcept {
    return stopped_.load(std::memory_order_acquire);
  }

  // Post work to the context (internal use)
  void post(std::function<void()> work) {
    {
      std::unique_lock lock(mutex_);
      work_queue_.push(std::move(work));
    }
    cv_.notify_one();
  }

  // Forward declaration
  struct schedule_sender;

  // Scheduler implementation
  class scheduler_type {
   public:
    using scheduler_concept = execution::scheduler_t;

    explicit scheduler_type(io_context& ctx) noexcept : ctx_(&ctx) {}

    // Get unique context identifier for socket compatibility checking (P2762R2 §9.1.2.6)
    [[nodiscard]] std::uintptr_t context_id() const noexcept {
      return ctx_->context_id();
    }

    [[nodiscard]] bool operator==(scheduler_type const& other) const noexcept {
      return ctx_ == other.ctx_;
    }

    // Schedule operation
    [[nodiscard]] auto schedule() const noexcept -> schedule_sender;

   private:
    io_context* ctx_;
  };

 private:
  std::uintptr_t                    context_id_;
  std::atomic<bool>                 stopped_;
  std::mutex                        mutex_;
  std::condition_variable           cv_;
  std::queue<std::function<void()>> work_queue_;
};

// Schedule sender implementation (defined outside to avoid local class template issues)
struct io_context::schedule_sender {
  using sender_concept = execution::sender_t;
  using value_types    = std::__type_list<std::__type_list<>>;

  io_context* ctx_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const noexcept {
    return execution::completion_signatures<execution::set_value_t(),
                                            execution::set_error_t(std::exception_ptr),
                                            execution::set_stopped_t()>{};
  }

  template <execution::receiver R>
  struct operation_state {
    using operation_state_concept = execution::operation_state_t;

    io_context* ctx_;
    R           receiver_;

    void start() & noexcept {
      try {
        ctx_->post([this]() {
          try {
            std::move(receiver_).set_value();
          } catch (...) {
            std::move(receiver_).set_error(std::current_exception());
          }
        });
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }
  };

  template <execution::receiver R>
  auto connect(R&& r) && noexcept {
    return operation_state<std::remove_cvref_t<R>>{ctx_, std::forward<R>(r)};
  }

  template <execution::receiver R>
  auto connect(R&& r) const& noexcept {
    return operation_state<std::remove_cvref_t<R>>{ctx_, std::forward<R>(r)};
  }
};

inline auto io_context::scheduler_type::schedule() const noexcept -> schedule_sender {
  return schedule_sender{ctx_};
}

inline io_context::scheduler_type io_context::get_scheduler() noexcept {
  return scheduler_type(*this);
}

// Verify io_scheduler concept
static_assert(io_scheduler<io_context::scheduler_type>);

}  // namespace flow::net
