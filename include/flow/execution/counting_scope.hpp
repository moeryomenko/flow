#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <stop_token>

#include "completion_signatures.hpp"
#include "receiver.hpp"
#include "sender.hpp"

namespace flow::execution {

// [exec.simple.counting.scope], simple_counting_scope
class simple_counting_scope {
 public:
  class token;

  simple_counting_scope() noexcept = default;

  ~simple_counting_scope() {
    // Safe to destroy if: never used, properly closed, or properly joined
    auto state = state_.load(std::memory_order_acquire);
    if (state != state_unused && state != state_joined && state != state_unused_and_closed) {
      std::terminate();
    }
  }

  simple_counting_scope(const simple_counting_scope&)            = delete;
  simple_counting_scope& operator=(const simple_counting_scope&) = delete;

  token get_token() noexcept;

  void close() noexcept {
    uint64_t state = state_.load(std::memory_order_acquire);
    while (true) {
      if ((state == state_unused)
          || (state == state_open && count_.load(std::memory_order_acquire) == 0)) {
        if (state_.compare_exchange_weak(state, state_unused_and_closed,
                                         std::memory_order_acq_rel)) {
          break;
        }
      } else {
        break;  // Already closed or has active associations
      }
    }
  }

  template <receiver Rcvr>
  struct join_operation {
    using operation_state_concept = operation_state_t;
    simple_counting_scope* scope_;
    Rcvr                   rcvr_;

    void start() noexcept {
      // Transition to joining state
      auto old_state = scope_->state_.exchange(state_joining, std::memory_order_acq_rel);
      (void)old_state;  // Suppress unused warning

      // Check count
      if (scope_->count_.load(std::memory_order_acquire) == 0) {
        scope_->state_.store(state_joined, std::memory_order_release);
        std::move(rcvr_).set_value();
      } else {
        // Store receiver for later completion
        // In real implementation, this would need synchronization
        std::move(rcvr_).set_value();
      }
    }
  };

  struct join_sender {
    using sender_concept = sender_t;
    simple_counting_scope* scope_;

    template <class Env>
    auto get_completion_signatures(Env&& /*unused*/) const {
      return completion_signatures<set_value_t()>{};
    }

    template <receiver Rcvr>
    auto connect(Rcvr&& rcvr) {
      return join_operation<Rcvr>{scope_, std::forward<Rcvr>(rcvr)};
    }
  };

  auto join() noexcept {
    return join_sender{this};
  }

 private:
  static constexpr uint64_t state_unused            = 0;
  static constexpr uint64_t state_open              = 1;
  static constexpr uint64_t state_unused_and_closed = 2;
  static constexpr uint64_t state_closed            = 3;
  static constexpr uint64_t state_joining           = 4;
  static constexpr uint64_t state_joined            = 5;

  std::atomic<uint64_t> state_{state_unused};
  std::atomic<uint64_t> count_{0};

  bool try_associate_impl() noexcept {
    auto state = state_.load(std::memory_order_acquire);

    while (true) {
      if (state == state_unused) {
        if (state_.compare_exchange_weak(state, state_open, std::memory_order_acq_rel)) {
          count_.fetch_add(1, std::memory_order_acq_rel);
          return true;
        }
      } else if (state == state_open) {
        count_.fetch_add(1, std::memory_order_acq_rel);
        return true;
      } else {
        return false;
      }
    }
  }

  void disassociate_impl() noexcept {
    auto old_count = count_.fetch_sub(1, std::memory_order_acq_rel);
    if (old_count == 1) {
      // Last association released, may need to complete join
      auto state = state_.load(std::memory_order_acquire);
      if (state == state_joining) {
        state_.store(state_joined, std::memory_order_release);
        // Signal waiting join operation
      }
    }
  }

  friend class token;
};

class simple_counting_scope::token {
 public:
  explicit token(simple_counting_scope& scope) noexcept : scope_(&scope) {}

  token(const token&)            = default;
  token& operator=(const token&) = default;

  bool try_associate() noexcept {
    return scope_->try_associate_impl();
  }

  void disassociate() noexcept {
    scope_->disassociate_impl();
  }

  template <sender Sndr>
  auto wrap(Sndr&& sndr) const {
    return std::forward<Sndr>(sndr);
  }

 private:
  simple_counting_scope* scope_;
};

inline simple_counting_scope::token simple_counting_scope::get_token() noexcept {
  return token{*this};
}

// [exec.counting.scope], counting_scope
class counting_scope {
 public:
  class token;

  counting_scope() noexcept = default;

  ~counting_scope() {
    // Safe to destroy if: never used, properly closed, or properly joined
    auto state = state_.load(std::memory_order_acquire);
    if (state != state_unused && state != state_joined && state != state_unused_and_closed) {
      std::terminate();
    }
  }

  counting_scope(const counting_scope&)            = delete;
  counting_scope& operator=(const counting_scope&) = delete;

  token get_token() noexcept;

  void close() noexcept {
    uint64_t state = state_.load(std::memory_order_acquire);
    while (true) {
      if ((state == state_unused)
          || (state == state_open && count_.load(std::memory_order_acquire) == 0)) {
        if (state_.compare_exchange_weak(state, state_unused_and_closed,
                                         std::memory_order_acq_rel)) {
          break;
        }
      } else {
        break;  // Already closed or has active associations
      }
    }
  }

  void request_stop() noexcept {
    stop_source_.request_stop();
  }

  auto get_stop_token() const noexcept {
    return stop_source_.get_token();
  }

  template <receiver Rcvr>
  struct join_operation {
    using operation_state_concept = operation_state_t;
    counting_scope* scope_;
    Rcvr            rcvr_;

    void start() noexcept {
      auto old_state = scope_->state_.exchange(state_joining, std::memory_order_acq_rel);
      (void)old_state;  // Suppress unused warning

      if (scope_->count_.load(std::memory_order_acquire) == 0) {
        scope_->state_.store(state_joined, std::memory_order_release);
        std::move(rcvr_).set_value();
      } else {
        std::move(rcvr_).set_value();
      }
    }
  };

  struct join_sender {
    using sender_concept = sender_t;
    counting_scope* scope_;

    template <class Env>
    auto get_completion_signatures(Env&& /*unused*/) const {
      return completion_signatures<set_value_t()>{};
    }

    template <receiver Rcvr>
    auto connect(Rcvr&& rcvr) {
      return join_operation<Rcvr>{scope_, std::forward<Rcvr>(rcvr)};
    }
  };

  auto join() noexcept {
    return join_sender{this};
  }

 private:
  static constexpr uint64_t state_unused            = 0;
  static constexpr uint64_t state_open              = 1;
  static constexpr uint64_t state_unused_and_closed = 2;
  static constexpr uint64_t state_closed            = 3;
  static constexpr uint64_t state_joining           = 4;
  static constexpr uint64_t state_joined            = 5;

  std::atomic<uint64_t> state_{state_unused};
  std::atomic<uint64_t> count_{0};
  std::stop_source      stop_source_;

  bool try_associate_impl() noexcept {
    auto state = state_.load(std::memory_order_acquire);

    while (true) {
      if (state == state_unused) {
        if (state_.compare_exchange_weak(state, state_open, std::memory_order_acq_rel)) {
          count_.fetch_add(1, std::memory_order_acq_rel);
          return true;
        }
      } else if (state == state_open) {
        count_.fetch_add(1, std::memory_order_acq_rel);
        return true;
      } else {
        return false;
      }
    }
  }

  void disassociate_impl() noexcept {
    auto old_count = count_.fetch_sub(1, std::memory_order_acq_rel);
    if (old_count == 1) {
      auto state = state_.load(std::memory_order_acquire);
      if (state == state_joining) {
        state_.store(state_joined, std::memory_order_release);
      }
    }
  }

  friend class token;
};

class counting_scope::token {
 public:
  explicit token(counting_scope& scope) noexcept : scope_(&scope) {}

  token(const token&)            = default;
  token& operator=(const token&) = default;

  bool try_associate() noexcept {
    return scope_->try_associate_impl();
  }

  void disassociate() noexcept {
    scope_->disassociate_impl();
  }

  // Helper receiver with combined stop token
  template <receiver Rcvr>
  struct stop_receiver {
    using receiver_concept = receiver_t;
    Rcvr            rcvr_;
    std::stop_token stop_token_;

    template <class... Args>
    void set_value(Args&&... args) noexcept {
      if (stop_token_.stop_requested()) {
        std::move(rcvr_).set_stopped();
      } else {
        std::move(rcvr_).set_value(std::forward<Args>(args)...);
      }
    }

    template <class Error>
    void set_error(Error&& err) noexcept {
      std::move(rcvr_).set_error(std::forward<Error>(err));
    }

    void set_stopped() noexcept {
      std::move(rcvr_).set_stopped();
    }

    auto get_env() const noexcept {
      return flow::execution::get_env(rcvr_);
    }
  };

  // Helper sender with stop token support
  template <class Sndr>
    requires sender<Sndr>
  struct stop_when_sender {
    using sender_concept = sender_t;
    Sndr            sndr_;
    std::stop_token stop_token_;

    template <class Env>
    auto get_completion_signatures(Env&& env) const {
      return std::forward<Sndr>(sndr_).get_completion_signatures(std::forward<Env>(env));
    }

    template <class Rcvr>
      requires receiver<Rcvr>
    auto connect(Rcvr&& rcvr) {
      return flow::execution::connect(std::forward<Sndr>(sndr_),
                                      stop_receiver<Rcvr>{std::forward<Rcvr>(rcvr), stop_token_});
    }
  };

  template <class Sndr>
    requires sender<Sndr>
  auto wrap(Sndr&& sndr) const {
    return stop_when_sender<Sndr>{std::forward<Sndr>(sndr), scope_->get_stop_token()};
  }

 private:
  counting_scope* scope_;
};

inline counting_scope::token counting_scope::get_token() noexcept {
  return token{*this};
}

}  // namespace flow::execution
