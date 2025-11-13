#pragma once

#include <atomic>
#include <exception>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "completion_signatures.hpp"
#include "env.hpp"
#include "receiver.hpp"
#include "scope_concepts.hpp"
#include "sender.hpp"
#include "utils.hpp"

namespace flow::execution {

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
  auto operator()(Sndr&& sndr, Token token, Env&& /*env*/) const {
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

}  // namespace flow::execution
