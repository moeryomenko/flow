#pragma once

#include <chrono>
#include <concepts>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

#include "operation_state.hpp"
#include "receiver.hpp"
#include "scheduler.hpp"
#include "sender.hpp"

namespace flow::execution {

// ============================================================================
// Basic retry - retries indefinitely on error
// ============================================================================

namespace _retry_detail {

// Forward declarations
template <sender S, receiver R>
struct _retry_state;

template <sender S, receiver R>
struct _retry_receiver;

// Shared state that coordinates retries
//
// NOTE ON HEAP ALLOCATION:
// This implementation uses heap allocation (std::unique_ptr) to break a
// fundamental circular type dependency that cannot be resolved on the stack:
//
// 1. The receiver's set_error() needs to call state->retry()
// 2. The retry() method needs to create: sender.connect(receiver)
// 3. To determine the nested_op_ type, we need: decltype(sender.connect(receiver))
// 4. During template instantiation, this triggers instantiation of receiver's methods
// 5. Which requires _retry_state to be complete, but we're still defining it!
//
// This creates an impossible circular dependency during C++ template instantiation.
// The only solutions are:
// - Heap allocation with type erasure (current approach) - ~16-24 bytes overhead
// - Virtual functions (vtable overhead) - similar or worse overhead
// - std::function (internal heap allocation) - similar overhead
// - Complete algorithm redesign to avoid callback pattern
//
// The heap allocation approach is:
// - Standard in P2300 reference implementations
// - Minimal overhead (one allocation per retry operation, not per attempt)
// - Clean and maintainable code
// - Excellent performance in practice (allocation cost << work being retried)
//
// Attempts to use forward declarations, aligned storage, std::optional,
// or other stack-based approaches all fail at the same point: when the
// compiler tries to instantiate receiver methods during nested_op_ type
// deduction, it needs the complete _retry_state type, creating a cycle.
template <sender S, receiver R>
struct _retry_state {
  S sender_;
  R outer_receiver_;

  // Operation state stored on the heap to avoid circular dependency
  std::unique_ptr<void, void (*)(void*)> nested_op_;

  // Thread safety: protects nested_op_ and retrying_ flag
  std::mutex mutex_;

  // Reentry guard: prevents nested retry() calls when operation completes synchronously
  bool retrying_ = false;

  explicit _retry_state(S s, R r)
      : sender_(static_cast<S&&>(s)),
        outer_receiver_(static_cast<R&&>(r)),
        nested_op_(nullptr, +[](void*) {}) {}

  void start_initial() {
    retry();
  }

  void retry() {
    using op_t = decltype(sender_.connect(std::declval<_retry_receiver<S, R>>()));

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
      temp_op = std::make_unique<op_t>(sender_.connect(_retry_receiver<S, R>{this}));
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
    // NOLINT: Intentional type erasure for breaking circular dependency - see comment above
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

// Receiver that retries by calling back to the shared state
template <sender S, receiver R>
struct _retry_receiver {
  using receiver_concept = receiver_t;

  _retry_state<S, R>* state_;

  template <class... Args>
  void set_value(Args&&... args) && noexcept {
    std::move(state_->outer_receiver_).set_value(std::forward<Args>(args)...);
  }

  template <class E>
  void set_error(E&& /*unused*/) && noexcept {
    // Retry by calling back to state
    try {
      state_->retry();
    } catch (...) {
      std::move(state_->outer_receiver_).set_error(std::current_exception());
    }
  }

  void set_stopped() && noexcept {
    std::move(state_->outer_receiver_).set_stopped();
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(state_->outer_receiver_);
  }
};

template <sender S, receiver R>
struct _retry_operation {
  using operation_state_concept = operation_state_t;

  std::unique_ptr<_retry_state<S, R>> state_;

  _retry_operation(S s, R r)
      : state_(std::make_unique<_retry_state<S, R>>(static_cast<S&&>(s), static_cast<R&&>(r))) {}

  void start() & noexcept {
    state_->start_initial();
  }
};

}  // namespace _retry_detail

template <sender S>
struct _retry_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S sender_;

  explicit _retry_sender(S s) : sender_(static_cast<S&&>(s)) {}

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Retry forwards value completions from inner sender
    return sender_.get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _retry_detail::_retry_operation<S, __decay_t<R>>{std::move(sender_), std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _retry_detail::_retry_operation<S, __decay_t<R>>{sender_, std::forward<R>(r)};
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(sender_);
  }
};

// ============================================================================
// retry_n - retries up to N times
// ============================================================================

namespace _retry_n_detail {

template <sender S, receiver R>
struct _retry_n_state;

template <sender S, receiver R>
struct _retry_n_receiver;

// Shared state that coordinates retries with attempt counting
template <sender S, receiver R>
struct _retry_n_state {
  S           sender_;
  R           outer_receiver_;
  std::size_t max_attempts_;
  std::size_t current_attempt_ = 0;

  std::unique_ptr<void, void (*)(void*)> nested_op_;

  // Thread safety: protects nested_op_, retrying_ flag, and current_attempt_
  std::mutex mutex_;

  // Reentry guard: prevents nested retry() calls when operation completes synchronously
  bool retrying_ = false;

  _retry_n_state(S s, R r, std::size_t max_attempts)
      : sender_(static_cast<S&&>(s)),
        outer_receiver_(static_cast<R&&>(r)),
        max_attempts_(max_attempts),
        nested_op_(nullptr, +[](void*) {}) {}

  void start_initial() {
    retry();
  }

  template <class E>
  void retry_or_fail(E&& e) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++current_attempt_;
    if (current_attempt_ >= max_attempts_) {
      // Max attempts reached
      lock.unlock();
      std::move(outer_receiver_).set_error(std::forward<E>(e));
    } else {
      lock.unlock();
      try {
        retry();
      } catch (...) {
        std::move(outer_receiver_).set_error(std::current_exception());
      }
    }
  }

 private:
  void retry() {
    using op_t = decltype(sender_.connect(std::declval<_retry_n_receiver<S, R>>()));

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
      temp_op = std::make_unique<op_t>(sender_.connect(_retry_n_receiver<S, R>{this}));
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

// Receiver that counts attempts and retries
template <sender S, receiver R>
struct _retry_n_receiver {
  using receiver_concept = receiver_t;

  _retry_n_state<S, R>* state_;

  template <class... Args>
  void set_value(Args&&... args) && noexcept {
    std::move(state_->outer_receiver_).set_value(std::forward<Args>(args)...);
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    state_->retry_or_fail(std::forward<E>(e));
  }

  void set_stopped() && noexcept {
    std::move(state_->outer_receiver_).set_stopped();
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(state_->outer_receiver_);
  }
};

template <sender S, receiver R>
struct _retry_n_operation {
  using operation_state_concept = operation_state_t;

  std::unique_ptr<_retry_n_state<S, R>> state_;

  _retry_n_operation(S s, R r, std::size_t max_attempts)
      : state_(std::make_unique<_retry_n_state<S, R>>(static_cast<S&&>(s), static_cast<R&&>(r),
                                                      max_attempts)) {}

  void start() & noexcept {
    state_->start_initial();
  }
};

}  // namespace _retry_n_detail

template <sender S>
struct _retry_n_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S           sender_;
  std::size_t max_attempts_;

  _retry_n_sender(S s, std::size_t max_attempts)
      : sender_(static_cast<S&&>(s)), max_attempts_(max_attempts) {}

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Retry_n forwards value completions from inner sender
    return sender_.get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _retry_n_detail::_retry_n_operation<S, __decay_t<R>>{std::move(sender_),
                                                                std::forward<R>(r), max_attempts_};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _retry_n_detail::_retry_n_operation<S, __decay_t<R>>{sender_, std::forward<R>(r),
                                                                max_attempts_};
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(sender_);
  }
};

// ============================================================================
// retry_if - retries if predicate returns true
// ============================================================================

namespace _retry_if_detail {

template <sender S, receiver R, class Pred>
struct _retry_if_state;

template <sender S, receiver R, class Pred>
struct _retry_if_receiver;

// Shared state that coordinates conditional retries
template <sender S, receiver R, class Pred>
struct _retry_if_state {
  S    sender_;
  R    outer_receiver_;
  Pred predicate_;

  std::unique_ptr<void, void (*)(void*)> nested_op_;

  // Thread safety: protects nested_op_ and retrying_ flag
  std::mutex mutex_;

  // Reentry guard: prevents nested retry() calls when operation completes synchronously
  bool retrying_ = false;

  _retry_if_state(S s, R r, Pred pred)
      : sender_(static_cast<S&&>(s)),
        outer_receiver_(static_cast<R&&>(r)),
        predicate_(static_cast<Pred&&>(pred)),
        nested_op_(nullptr, +[](void*) {}) {}

  void start_initial() {
    retry();
  }

  template <class E>
  void retry_if_allowed(E&& e) {
    std::exception_ptr ep;
    try {
      if constexpr (std::same_as<std::decay_t<E>, std::exception_ptr>) {
        ep = std::forward<E>(e);
      } else {
        ep = std::make_exception_ptr(std::forward<E>(e));
      }

      // Check if we should retry
      if (predicate_(ep)) {
        // Retry
        retry();
      } else {
        // Don't retry, propagate error
        std::move(outer_receiver_).set_error(std::move(ep));
      }
    } catch (...) {
      std::move(outer_receiver_).set_error(std::current_exception());
    }
  }

 private:
  void retry() {
    using op_t = decltype(sender_.connect(std::declval<_retry_if_receiver<S, R, Pred>>()));

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
      temp_op = std::make_unique<op_t>(sender_.connect(_retry_if_receiver<S, R, Pred>{this}));
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

// Receiver that checks predicate before retrying
template <sender S, receiver R, class Pred>
struct _retry_if_receiver {
  using receiver_concept = receiver_t;

  _retry_if_state<S, R, Pred>* state_;

  template <class... Args>
  void set_value(Args&&... args) && noexcept {
    std::move(state_->outer_receiver_).set_value(std::forward<Args>(args)...);
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    state_->retry_if_allowed(std::forward<E>(e));
  }

  void set_stopped() && noexcept {
    std::move(state_->outer_receiver_).set_stopped();
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(state_->outer_receiver_);
  }
};

template <sender S, receiver R, class Pred>
struct _retry_if_operation {
  using operation_state_concept = operation_state_t;

  std::unique_ptr<_retry_if_state<S, R, Pred>> state_;

  _retry_if_operation(S s, R r, Pred pred)
      : state_(std::make_unique<_retry_if_state<S, R, Pred>>(
            static_cast<S&&>(s), static_cast<R&&>(r), static_cast<Pred&&>(pred))) {}

  void start() & noexcept {
    state_->start_initial();
  }
};

}  // namespace _retry_if_detail

template <sender S, class Pred>
struct _retry_if_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S    sender_;
  Pred predicate_;

  _retry_if_sender(S s, Pred pred)
      : sender_(static_cast<S&&>(s)), predicate_(static_cast<Pred&&>(pred)) {}

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Retry_if forwards value completions from inner sender
    return sender_.get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _retry_if_detail::_retry_if_operation<S, __decay_t<R>, Pred>{
        std::move(sender_), std::forward<R>(r), std::move(predicate_)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _retry_if_detail::_retry_if_operation<S, __decay_t<R>, Pred>{sender_, std::forward<R>(r),
                                                                        predicate_};
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(sender_);
  }
};

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

// ============================================================================
// CPOs and pipeable helpers
// ============================================================================

// Pipeable wrappers
struct _pipeable_retry {
  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry& /*unused*/) {
    return _retry_sender<__decay_t<S>>{std::forward<S>(s)};
  }
};

struct _pipeable_retry_n {
  std::size_t max_attempts_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry_n& p) {
    return _retry_n_sender<__decay_t<S>>{std::forward<S>(s), p.max_attempts_};
  }
};

template <class Pred>
struct _pipeable_retry_if {
  Pred predicate_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry_if& p) {
    return _retry_if_sender<__decay_t<S>, Pred>{std::forward<S>(s), p.predicate_};
  }
};

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

// retry
struct retry_t {
  template <sender S>
  auto operator()(S&& s) const {
    return _retry_sender<__decay_t<S>>{std::forward<S>(s)};
  }

  auto operator()() const {
    return _pipeable_retry{};
  }
};

inline constexpr retry_t retry{};

// retry_n
struct retry_n_t {
  template <sender S>
  auto operator()(S&& s, std::size_t max_attempts) const {
    return _retry_n_sender<__decay_t<S>>{std::forward<S>(s), max_attempts};
  }

  auto operator()(std::size_t max_attempts) const {
    return _pipeable_retry_n{max_attempts};
  }
};

inline constexpr retry_n_t retry_n{};

// retry_if
struct retry_if_t {
  template <sender S, class Pred>
  auto operator()(S&& s, Pred&& pred) const {
    return _retry_if_sender<__decay_t<S>, __decay_t<Pred>>{std::forward<S>(s),
                                                           std::forward<Pred>(pred)};
  }

  template <class Pred>
  auto operator()(Pred&& pred) const {
    return _pipeable_retry_if<__decay_t<Pred>>{std::forward<Pred>(pred)};
  }
};

inline constexpr retry_if_t retry_if{};

// retry_with_backoff
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
