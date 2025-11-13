#pragma once

#include <exception>
#include <memory>
#include <mutex>

#include "operation_state.hpp"
#include "receiver.hpp"
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

// Pipeable wrapper
struct _pipeable_retry {
  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry& /*unused*/) {
    return _retry_sender<__decay_t<S>>{std::forward<S>(s)};
  }
};

// retry CPO
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

}  // namespace flow::execution
