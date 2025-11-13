#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "counting_scope.hpp"
#include "env.hpp"
#include "receiver.hpp"
#include "scope_association.hpp"
#include "sender.hpp"
#include "upon.hpp"
#include "utils.hpp"

namespace flow::execution {

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
