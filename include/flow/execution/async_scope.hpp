#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>

#include "completion_signatures.hpp"
#include "env.hpp"
#include "operation_state.hpp"
#include "queries.hpp"
#include "receiver.hpp"
#include "sender.hpp"
#include "utils.hpp"

namespace flow::execution {

// Helper void sender for concept checking
struct __void_sender_for_concept {
  using sender_concept = sender_t;

  template <class Env>
  auto get_completion_signatures(Env&&) const -> completion_signatures<set_value_t()>;

  template <class Rcvr>
  auto connect(Rcvr&& rcvr) -> void;
};

// [exec.scope.assoc], async scope association concept
template <class Assoc>
concept async_scope_association = std::semiregular<Assoc> && requires(Assoc assoc) {
  { assoc.is_associated() } noexcept -> std::same_as<bool>;
  { assoc.disassociate() } noexcept -> std::same_as<void>;
};

// [exec.scope.token], scope token concept
template <class Token>
concept scope_token =
    std::copyable<Token> && requires(Token token, __void_sender_for_concept sndr) {
      { token.try_associate() } -> std::same_as<bool>;
      { token.disassociate() } noexcept -> std::same_as<void>;
      { token.wrap(std::move(sndr)) } -> sender;
    };

// Helper void sender for actual use
struct void_sender {
  using sender_concept = sender_t;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    return completion_signatures<set_value_t()>{};
  }

  template <receiver Rcvr>
  auto connect(Rcvr&& rcvr) {
    struct operation {
      Rcvr rcvr_;
      void start() noexcept {
        std::move(rcvr_).set_value();
      }
    };
    return operation{std::forward<Rcvr>(rcvr)};
  }
};

// Implementation details namespace
namespace __async_scope {

// Helper to check if a type is an lvalue reference
template <class T>
constexpr bool is_lvalue_ref_v = std::is_lvalue_reference_v<T>;

// Storage for sender in nest operation
template <sender Sndr>
struct nest_data {
  using sender_type = __remove_cvref_t<Sndr>;

  std::optional<sender_type> sndr_;

  explicit nest_data(Sndr&& sndr) : sndr_(std::forward<Sndr>(sndr)) {}

  sender_type& get_sender() noexcept {
    return *sndr_;
  }

  void destroy_sender() noexcept {
    sndr_.reset();
  }
};

// Deduction guide for nest_data
template <sender Sndr>
nest_data(Sndr&&) -> nest_data<Sndr>;

// Receiver that manages scope association lifetime
template <class Rcvr, class Assoc>
struct nest_receiver {
  using receiver_concept = receiver_t;

  Rcvr  rcvr_;
  Assoc assoc_;

  template <class... Args>
  void set_value(Args&&... args) noexcept {
    assoc_.disassociate();
    std::move(rcvr_).set_value(std::forward<Args>(args)...);
  }

  template <class Error>
  void set_error(Error&& err) noexcept {
    assoc_.disassociate();
    std::move(rcvr_).set_error(std::forward<Error>(err));
  }

  void set_stopped() noexcept {
    assoc_.disassociate();
    std::move(rcvr_).set_stopped();
  }

  auto get_env() const noexcept {
    return flow::execution::get_env(rcvr_);
  }
};

// Association handle
template <class Token>
struct association {
  Token token_;
  bool  is_associated_{false};

  [[nodiscard]] bool is_associated() const noexcept {
    return is_associated_;
  }

  void disassociate() noexcept {
    if (is_associated_) {
      token_.disassociate();
      is_associated_ = false;
    }
  }
};

// Sender returned by associate
template <class Sndr, class Token>
struct nest_sender {
  using sender_concept = sender_t;

  Sndr  sndr_;
  Token token_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    return sndr_.get_completion_signatures(std::forward<Env>(env));
  }

  template <class Rcvr>
  auto connect(Rcvr&& rcvr) {
    using wrapped_rcvr_t = nest_receiver<Rcvr, association<Token>>;
    using wrapped_snd_t  = decltype(token_.wrap(std::declval<Sndr>()));
    using wrapped_op_t   = decltype(flow::execution::connect(std::declval<wrapped_snd_t>(),
                                                             std::declval<wrapped_rcvr_t>()));

    struct stopped_operation {
      using operation_state_concept = operation_state_t;
      Rcvr rcvr_;
      void start() noexcept {
        std::move(rcvr_).set_stopped();
      }
    };

    using operation_variant = std::variant<stopped_operation, wrapped_op_t>;

    struct operation_wrapper {
      using operation_state_concept = operation_state_t;
      operation_variant op_;

      void start() noexcept {
        std::visit([](auto&& op) { flow::execution::start(op); }, op_);
      }
    };

    if (!token_.try_associate()) {
      return operation_wrapper{
          operation_variant{std::in_place_index<0>, stopped_operation{std::forward<Rcvr>(rcvr)}}};
    }

    auto assoc        = association<Token>{token_, true};
    auto wrapped_rcvr = wrapped_rcvr_t{std::forward<Rcvr>(rcvr), std::move(assoc)};
    auto wrapped_snd  = token_.wrap(std::forward<Sndr>(sndr_));
    auto wrapped_op   = flow::execution::connect(std::move(wrapped_snd), std::move(wrapped_rcvr));

    return operation_wrapper{operation_variant{std::in_place_index<1>, std::move(wrapped_op)}};
  }
};

}  // namespace __async_scope

// [exec.assoc], associate customization point
struct associate_t {
  template <sender Sndr, scope_token Token>
  constexpr auto operator()(Sndr&& sndr, Token token) const {
    return __async_scope::nest_sender<Sndr, Token>{std::forward<Sndr>(sndr), token};
  }
};

inline constexpr associate_t associate{};

// [exec.spawn], spawn customization point
struct spawn_receiver {
  using receiver_concept = receiver_t;

  void set_value() noexcept {}
  void set_stopped() noexcept {}

  template <class E>
  void set_error(E&& /*unused*/) noexcept {}

  [[nodiscard]] static auto get_env() noexcept {
    return empty_env{};
  }
};

struct spawn_t {
  template <sender Sndr, scope_token Token>
  void operator()(Sndr&& sndr, Token token) const {
    auto associated = associate(std::forward<Sndr>(sndr), token);
    auto op         = flow::execution::connect(std::move(associated), spawn_receiver{});
    op.start();
  }

  template <sender Sndr, scope_token Token, class Env>
  void operator()(Sndr&& sndr, Token token, Env&& env) const {
    (void)env;  // Suppress unused warning for now
    auto associated = associate(std::forward<Sndr>(sndr), token);
    auto op         = flow::execution::connect(std::move(associated), spawn_receiver{});
    op.start();
  }
};

inline constexpr spawn_t spawn{};

// [exec.spawn.future], spawn_future implementation
namespace __spawn_future {

template <class... Ts>
using decayed_tuple = std::tuple<std::decay_t<Ts>...>;

template <class... Sigs>
struct result_variant;

template <class... Ts>
struct result_variant<completion_signatures<Ts...>> {
  using type = std::variant<std::monostate, decayed_tuple<Ts>...>;
};

template <class Sndr, class Env>
using result_variant_t =
    typename result_variant<decltype(std::declval<Sndr>().get_completion_signatures(
        std::declval<Env>()))>::type;

template <class Result>
struct shared_state {
  std::atomic<bool>  completed_{false};
  Result             result_;
  std::exception_ptr exception_;
};

template <class Result>
struct future_receiver {
  using receiver_concept = receiver_t;

  std::shared_ptr<shared_state<Result>> state_;

  template <class... Args>
  void set_value(Args&&... args) noexcept {
    try {
      state_->result_ = decayed_tuple<Args...>{std::forward<Args>(args)...};
      state_->completed_.store(true, std::memory_order_release);
    } catch (...) {
      state_->exception_ = std::current_exception();
      state_->completed_.store(true, std::memory_order_release);
    }
  }

  template <class Error>
  void set_error(Error&& err) noexcept {
    state_->exception_ = std::make_exception_ptr(std::forward<Error>(err));
    state_->completed_.store(true, std::memory_order_release);
  }

  void set_stopped() noexcept {
    state_->completed_.store(true, std::memory_order_release);
  }

  auto get_env() const noexcept {
    return empty_env{};
  }
};

template <class Result>
struct future_sender {
  using sender_concept = sender_t;

  std::shared_ptr<shared_state<Result>> state_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Future sender can complete with any of the original completions
    return completion_signatures<>{};
  }

  template <receiver Rcvr>
  auto connect(Rcvr&& rcvr) {
    struct future_operation {
      std::shared_ptr<shared_state<Result>> state_;
      Rcvr                                  rcvr_;

      void start() noexcept {
        if (state_->completed_.load(std::memory_order_acquire)) {
          if (state_->exception_) {
            std::move(rcvr_).set_error(state_->exception_);
          } else {
            // Visit the result variant and forward to receiver
            std::visit(
                [this](auto&& result) {
                  if constexpr (!std::is_same_v<std::decay_t<decltype(result)>, std::monostate>) {
                    std::apply(
                        [this](auto&&... args) {
                          std::move(rcvr_).set_value(std::forward<decltype(args)>(args)...);
                        },
                        std::forward<decltype(result)>(result));
                  } else {
                    std::move(rcvr_).set_stopped();
                  }
                },
                state_->result_);
          }
        } else {
          std::move(rcvr_).set_stopped();
        }
      }
    };

    return future_operation{state_, std::forward<Rcvr>(rcvr)};
  }
};

}  // namespace __spawn_future

struct spawn_future_t {
  template <sender Sndr, scope_token Token>
  auto operator()(Sndr&& sndr, Token token) const {
    using result_t = __spawn_future::result_variant_t<Sndr, empty_env>;
    auto state     = std::make_shared<__spawn_future::shared_state<result_t>>();

    auto future_rcvr = __spawn_future::future_receiver<result_t>{state};
    auto associated  = associate(std::forward<Sndr>(sndr), token);
    auto op          = flow::execution::connect(std::move(associated), std::move(future_rcvr));
    op.start();

    return __spawn_future::future_sender<result_t>{state};
  }

  template <sender Sndr, scope_token Token, class Env>
  auto operator()(Sndr&& sndr, Token token, Env&& env) const {
    using result_t = __spawn_future::result_variant_t<Sndr, Env>;
    auto state     = std::make_shared<__spawn_future::shared_state<result_t>>();

    auto future_rcvr = __spawn_future::future_receiver<result_t>{state};
    auto associated  = associate(std::forward<Sndr>(sndr), token);
    auto op          = flow::execution::connect(std::move(associated), std::move(future_rcvr));
    op.start();

    return __spawn_future::future_sender<result_t>{state};
  }
};

inline constexpr spawn_future_t spawn_future{};

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

// ============================================================================
// let_async_scope implementation (P3296R4)
// ============================================================================

namespace __let_async_scope {

// Scope state that manages errors
template <class... Errors>
struct scope_state {
  counting_scope                          scope;
  std::mutex                              error_mutex;
  std::variant<std::monostate, Errors...> stored_error;
  bool                                    has_error{false};

  void store_error(auto&& error) {
    std::scoped_lock lock(error_mutex);
    if (!has_error) {
      stored_error = std::forward<decltype(error)>(error);
      has_error    = true;
      scope.request_stop();
    }
  }

  bool check_has_error() const {
    return has_error;
  }

  template <class Rcvr>
  void complete_with_error(Rcvr&& rcvr) {
    std::scoped_lock lock(error_mutex);
    std::visit(
        [&rcvr](auto&& error) {
          using error_t = decltype(error);
          if constexpr (!std::is_same_v<std::remove_cvref_t<error_t>, std::monostate>) {
            std::forward<Rcvr>(rcvr).set_error(std::forward<error_t>(error));
          }
        },
        std::move(stored_error));
  }
};

// Wrapper token that provides access to scope
template <class ScopeState>
struct error_intercepting_token {
  ScopeState*                    state_;
  typename counting_scope::token wrapped_token_;

  error_intercepting_token(ScopeState& state, typename counting_scope::token token)
      : state_(&state), wrapped_token_(token) {}

  bool try_associate() {
    return wrapped_token_.try_associate();
  }

  void disassociate() noexcept {
    wrapped_token_.disassociate();
  }

  template <sender Sndr>
  auto wrap(Sndr&& sndr) const {
    return wrapped_token_.wrap(std::forward<Sndr>(sndr));
  }

  auto get_stop_token() const noexcept {
    return state_->scope.get_stop_token();
  }

  // Tag invoke customization for spawn_t to intercept errors
  template <sender Sndr>
  friend void tag_invoke(spawn_t /*unused*/, Sndr&& sndr, error_intercepting_token token) {
    auto error_handling_sndr =
        std::forward<Sndr>(sndr) | upon_error([state = token.state_](auto&& error) {
          state->store_error(std::forward<decltype(error)>(error));
        });

    spawn(std::move(error_handling_sndr), token.wrapped_token_);
  }

  template <sender Sndr, class Env>
  friend void tag_invoke(spawn_t /*unused*/, Sndr&& sndr, error_intercepting_token token,
                         Env&& env) {
    auto error_handling_sndr =
        std::forward<Sndr>(sndr) | upon_error([state = token.state_](auto&& error) {
          state->store_error(std::forward<decltype(error)>(error));
        });

    spawn(std::move(error_handling_sndr), token.wrapped_token_, std::forward<Env>(env));
  }
};

// Receiver that wraps the final receiver
template <class Rcvr, class ScopeState>
struct join_receiver {
  using receiver_concept = receiver_t;

  Rcvr                        rcvr_;
  std::shared_ptr<ScopeState> state_;

  void set_value() && noexcept {
    if (state_->check_has_error()) {
      state_->complete_with_error(std::move(rcvr_));
    } else {
      std::move(rcvr_).set_value();
    }
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    std::move(rcvr_).set_error(std::forward<E>(e));
  }

  void set_stopped() && noexcept {
    std::move(rcvr_).set_stopped();
  }
};

// Forward declaration for two-phase initialization
template <class Sndr, class F, class Rcvr, class ScopeState>
struct let_async_scope_operation;

// Receiver for the child sender (user function invocation)
template <class Sndr, class F, class Rcvr, class ScopeState>
struct child_receiver {
  using receiver_concept = receiver_t;

  let_async_scope_operation<Sndr, F, Rcvr, ScopeState>* op_;
  F                                                     fun_;
  std::shared_ptr<ScopeState>                           state_;

  template <class... Args>
  void set_value(Args&&... args) && noexcept {
    try {
      auto token = error_intercepting_token{*state_, state_->scope.get_token()};
      std::invoke(std::move(fun_), token, std::forward<Args>(args)...);
    } catch (...) {
      state_->store_error(std::current_exception());
      state_->scope.request_stop();
    }

    // Now join the scope
    op_->start_join();
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    std::move(op_->join_rcvr_.rcvr_).set_error(std::forward<E>(e));
  }

  void set_stopped() && noexcept {
    std::move(op_->join_rcvr_.rcvr_).set_stopped();
  }

  auto get_env() const noexcept {
    return empty_env{};
  }
};

// Operation state for let_async_scope (two-phase initialization)
template <class Sndr, class F, class Rcvr, class ScopeState>
struct let_async_scope_operation {
  using operation_state_concept = operation_state_t;

  using join_rcvr_t = join_receiver<Rcvr, ScopeState>;
  using join_sndr_t = decltype(std::declval<ScopeState>().scope.join());
  using join_op_t =
      decltype(flow::execution::connect(std::declval<join_sndr_t>(), std::declval<join_rcvr_t>()));
  using child_rcvr_t = child_receiver<Sndr, F, Rcvr, ScopeState>;
  using child_op_t   = decltype(std::declval<Sndr>().connect(std::declval<child_rcvr_t>()));

  Sndr                        sndr_;
  F                           fun_;
  std::shared_ptr<ScopeState> state_;
  join_rcvr_t                 join_rcvr_;
  std::optional<child_op_t>   child_op_;
  std::optional<join_op_t>    join_op_;

  // Constructor stores all needed data but doesn't create child_op yet
  let_async_scope_operation(Sndr&& sndr, F&& fun, Rcvr&& rcvr, std::shared_ptr<ScopeState> state)
      : sndr_(std::forward<Sndr>(sndr)),
        fun_(std::forward<F>(fun)),
        state_(std::move(state)),
        join_rcvr_{std::forward<Rcvr>(rcvr), state_},
        child_op_(std::nullopt),
        join_op_(std::nullopt) {}

  void start() noexcept {
    // Phase 2: Now that operation exists, create child_op with correct 'this' pointer
    child_op_.emplace(std::move(sndr_).connect(child_rcvr_t{this, std::move(fun_), state_}));
    flow::execution::start(*child_op_);
  }

  void start_join() noexcept {
    auto join_sndr = state_->scope.join();
    join_op_.emplace(flow::execution::connect(std::move(join_sndr), std::move(join_rcvr_)));
    flow::execution::start(*join_op_);
  }
};

// The let_async_scope sender
template <sender Sndr, class F, class... Errors>
struct let_async_scope_sender_impl {
  using sender_concept = sender_t;

  Sndr sndr_;
  F    fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    if constexpr (sizeof...(Errors) == 0) {
      return completion_signatures<set_value_t(), set_error_t(std::exception_ptr),
                                   set_stopped_t()>{};
    } else {
      return completion_signatures<set_value_t(), set_error_t(Errors)..., set_stopped_t()>{};
    }
  }

  template <receiver Rcvr>
  auto connect(Rcvr&& rcvr) {
    using scope_state_t = scope_state<Errors...>;
    auto state          = std::make_shared<scope_state_t>();

    return let_async_scope_operation<Sndr, F, __remove_cvref_t<Rcvr>, scope_state_t>{
        std::move(sndr_), std::move(fun_), std::forward<Rcvr>(rcvr), std::move(state)};
  }
};

}  // namespace __let_async_scope

// Pipeable wrappers
template <class F>
struct _pipeable_let_async_scope {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_let_async_scope& p) {
    return __let_async_scope::let_async_scope_sender_impl<__remove_cvref_t<S>, F,
                                                          std::exception_ptr>{std::forward<S>(s),
                                                                              p.fun_};
  }
};

template <class F, class... Errors>
struct _pipeable_let_async_scope_with_error {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_let_async_scope_with_error& p) {
    return __let_async_scope::let_async_scope_sender_impl<__remove_cvref_t<S>, F, Errors...>{
        std::forward<S>(s), p.fun_};
  }
};

// let_async_scope CPO
struct let_async_scope_t {
  template <sender Sndr, class F>
  auto operator()(Sndr&& sndr, F&& fun) const {
    return __let_async_scope::let_async_scope_sender_impl<__remove_cvref_t<Sndr>,
                                                          __remove_cvref_t<F>, std::exception_ptr>{
        std::forward<Sndr>(sndr), std::forward<F>(fun)};
  }

  template <class F>
  auto operator()(F&& fun) const {
    return _pipeable_let_async_scope<__remove_cvref_t<F>>{std::forward<F>(fun)};
  }
};

inline constexpr let_async_scope_t let_async_scope{};

// let_async_scope_with_error CPO
template <class... Errors>
struct let_async_scope_with_error_t {
  template <sender Sndr, class F>
  auto operator()(Sndr&& sndr, F&& fun) const {
    return __let_async_scope::let_async_scope_sender_impl<__remove_cvref_t<Sndr>,
                                                          __remove_cvref_t<F>, Errors...>{
        std::forward<Sndr>(sndr), std::forward<F>(fun)};
  }

  template <class F>
  auto operator()(F&& fun) const {
    return _pipeable_let_async_scope_with_error<__remove_cvref_t<F>, Errors...>{
        std::forward<F>(fun)};
  }
};

template <class... Errors>
inline constexpr let_async_scope_with_error_t<Errors...> let_async_scope_with_error{};

}  // namespace flow::execution
