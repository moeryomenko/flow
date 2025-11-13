#pragma once

#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

#include "operation_state.hpp"
#include "receiver.hpp"
#include "scheduler.hpp"
#include "sender.hpp"

namespace flow::execution {

// ============================================================================
// retry_with_backoff - retries with exponential backoff
// ============================================================================

namespace _retry_with_backoff_detail {

template <sender S, receiver R, scheduler Sch>
struct _retry_with_backoff_state;

template <sender S, receiver R, scheduler Sch>
struct _retry_with_backoff_receiver;

// Shared state that coordinates retries with backoff
template <sender S, receiver R, scheduler Sch>
struct _retry_with_backoff_state {
  S                         sender_;
  R                         outer_receiver_;
  Sch                       scheduler_;
  std::chrono::milliseconds initial_delay_;
  std::chrono::milliseconds max_delay_;
  double                    multiplier_;
  std::size_t               max_attempts_;
  std::size_t               current_attempt_ = 0;
  std::chrono::milliseconds current_delay_;

  std::unique_ptr<void, void (*)(void*)> nested_op_;

  // Thread safety: protects nested_op_, retrying_ flag, current_attempt_, and current_delay_
  std::mutex mutex_;

  // Reentry guard: prevents nested retry() calls when operation completes synchronously
  bool retrying_ = false;

  _retry_with_backoff_state(S s, R r, Sch sch, std::chrono::milliseconds initial_delay,
                            std::chrono::milliseconds max_delay, double multiplier,
                            std::size_t max_attempts)
      : sender_(static_cast<S&&>(s)),
        outer_receiver_(static_cast<R&&>(r)),
        scheduler_(static_cast<Sch&&>(sch)),
        initial_delay_(initial_delay),
        max_delay_(max_delay),
        multiplier_(multiplier),
        max_attempts_(max_attempts),
        current_delay_(initial_delay),
        nested_op_(nullptr, +[](void*) {}) {}

  void start_initial() {
    retry();
  }

  template <class E>
  void retry_with_delay(E&& e) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++current_attempt_;
    if (current_attempt_ >= max_attempts_) {
      // Max attempts reached
      lock.unlock();
      std::move(outer_receiver_).set_error(std::forward<E>(e));
    } else {
      // Apply backoff delay (simple blocking sleep)
      auto delay = current_delay_;

      // Calculate next delay
      auto next_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double, std::milli>(current_delay_.count() * multiplier_));
      current_delay_ = std::min(next_delay, max_delay_);

      lock.unlock();

      std::this_thread::sleep_for(delay);

      try {
        // Retry
        retry();
      } catch (...) {
        std::move(outer_receiver_).set_error(std::current_exception());
      }
    }
  }

 private:
  void retry() {
    using op_t = decltype(sender_.connect(std::declval<_retry_with_backoff_receiver<S, R, Sch>>()));

    // Lock to prevent race conditions when operations complete on different threads
    std::unique_lock<std::mutex> lock(mutex_);

    // Prevent nested retry() calls (synchronous completion during start())
    if (retrying_) {
      return;
    }
    retrying_ = true;

    // Keep old operation alive until after new one starts (prevents use-after-free)
    auto old_op = std::move(nested_op_);

    // Exception-safe allocation: use unique_ptr wrapper to prevent leaks
    std::unique_ptr<op_t> temp_op;
    try {
      // Unlock during potentially long connect() operation
      lock.unlock();
      temp_op =
          std::make_unique<op_t>(sender_.connect(_retry_with_backoff_receiver<S, R, Sch>{this}));
      lock.lock();
    } catch (...) {
      // Relock if needed and reset flag before propagating
      if (!lock.owns_lock()) {
        lock.lock();
      }
      retrying_ = false;
      throw;
    }

    // Transfer ownership to type-erased unique_ptr
    // NOLINT: Intentional type erasure for breaking circular dependency
    auto* op_ptr = temp_op.release();  // NOLINT(cppcoreguidelines-owning-memory)
    nested_op_   = std::unique_ptr<void, void (*)(void*)>(
        op_ptr,
        +[](void* p) { delete static_cast<op_t*>(p); });  // NOLINT(cppcoreguidelines-owning-memory)

    retrying_ = false;
    lock.unlock();

    // Start outside the lock to avoid holding it during potentially long operation
    static_cast<op_t*>(nested_op_.get())->start();

    // old_op destroys here, after new operation has started
  }
};

// Receiver that handles retries with backoff delay
template <sender S, receiver R, scheduler Sch>
struct _retry_with_backoff_receiver {
  using receiver_concept = receiver_t;

  _retry_with_backoff_state<S, R, Sch>* state_;

  template <class... Args>
  void set_value(Args&&... args) && noexcept {
    std::move(state_->outer_receiver_).set_value(std::forward<Args>(args)...);
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    state_->retry_with_delay(std::forward<E>(e));
  }

  void set_stopped() && noexcept {
    std::move(state_->outer_receiver_).set_stopped();
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(state_->outer_receiver_);
  }
};

template <sender S, receiver R, scheduler Sch>
struct _retry_with_backoff_operation {
  using operation_state_concept = operation_state_t;

  std::unique_ptr<_retry_with_backoff_state<S, R, Sch>> state_;

  _retry_with_backoff_operation(S s, R r, Sch sch, std::chrono::milliseconds initial_delay,
                                std::chrono::milliseconds max_delay, double multiplier,
                                std::size_t max_attempts)
      : state_(std::make_unique<_retry_with_backoff_state<S, R, Sch>>(
            static_cast<S&&>(s), static_cast<R&&>(r), static_cast<Sch&&>(sch), initial_delay,
            max_delay, multiplier, max_attempts)) {}

  void start() & noexcept {
    state_->start_initial();
  }
};

}  // namespace _retry_with_backoff_detail

template <sender S, scheduler Sch>
struct _retry_with_backoff_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S                         sender_;
  Sch                       scheduler_;
  std::chrono::milliseconds initial_delay_;
  std::chrono::milliseconds max_delay_;
  double                    multiplier_;
  std::size_t               max_attempts_;

  _retry_with_backoff_sender(S s, Sch sch, std::chrono::milliseconds initial_delay,
                             std::chrono::milliseconds max_delay, double multiplier,
                             std::size_t max_attempts)
      : sender_(static_cast<S&&>(s)),
        scheduler_(static_cast<Sch&&>(sch)),
        initial_delay_(initial_delay),
        max_delay_(max_delay),
        multiplier_(multiplier),
        max_attempts_(max_attempts) {}

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Retry_with_backoff forwards value completions from inner sender
    return sender_.get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _retry_with_backoff_detail::_retry_with_backoff_operation<S, __decay_t<R>, Sch>{
        std::move(sender_), std::forward<R>(r), std::move(scheduler_), initial_delay_,
        max_delay_,         multiplier_,        max_attempts_};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _retry_with_backoff_detail::_retry_with_backoff_operation<S, __decay_t<R>, Sch>{
        sender_,    std::forward<R>(r), scheduler_,   initial_delay_,
        max_delay_, multiplier_,        max_attempts_};
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(sender_);
  }
};

// Pipeable wrapper
template <scheduler Sch>
struct _pipeable_retry_with_backoff {
  Sch                       scheduler_;
  std::chrono::milliseconds initial_delay_;
  std::chrono::milliseconds max_delay_;
  double                    multiplier_;
  std::size_t               max_attempts_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry_with_backoff& p) {
    return _retry_with_backoff_sender<__decay_t<S>, Sch>{std::forward<S>(s), p.scheduler_,
                                                         p.initial_delay_,   p.max_delay_,
                                                         p.multiplier_,      p.max_attempts_};
  }
};

// retry_with_backoff CPO
struct retry_with_backoff_t {
  template <sender S, scheduler Sch>
  auto operator()(S&& s, Sch&& sch, std::chrono::milliseconds initial_delay,
                  std::chrono::milliseconds max_delay, double multiplier,
                  std::size_t max_attempts) const {
    return _retry_with_backoff_sender<__decay_t<S>, __decay_t<Sch>>{
        std::forward<S>(s), std::forward<Sch>(sch), initial_delay, max_delay, multiplier,
        max_attempts};
  }

  template <scheduler Sch>
  auto operator()(Sch&&                     sch,
                  std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100),
                  std::chrono::milliseconds max_delay     = std::chrono::milliseconds(10000),
                  double multiplier = 2.0, std::size_t max_attempts = 5) const {
    return _pipeable_retry_with_backoff<__decay_t<Sch>>{std::forward<Sch>(sch), initial_delay,
                                                        max_delay, multiplier, max_attempts};
  }
};

inline constexpr retry_with_backoff_t retry_with_backoff{};

}  // namespace flow::execution
