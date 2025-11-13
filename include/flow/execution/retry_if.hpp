#pragma once

#include <concepts>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>

#include "operation_state.hpp"
#include "receiver.hpp"
#include "sender.hpp"

namespace flow::execution {

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

// Pipeable wrapper
template <class Pred>
struct _pipeable_retry_if {
  Pred predicate_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry_if& p) {
    return _retry_if_sender<__decay_t<S>, Pred>{std::forward<S>(s), p.predicate_};
  }
};

// retry_if CPO
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

}  // namespace flow::execution
