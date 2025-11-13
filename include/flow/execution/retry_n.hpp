#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>

#include "operation_state.hpp"
#include "receiver.hpp"
#include "sender.hpp"

namespace flow::execution {

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

// Pipeable wrapper
struct _pipeable_retry_n {
  std::size_t max_attempts_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_retry_n& p) {
    return _retry_n_sender<__decay_t<S>>{std::forward<S>(s), p.max_attempts_};
  }
};

// retry_n CPO
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

}  // namespace flow::execution
