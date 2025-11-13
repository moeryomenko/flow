#pragma once

#include <atomic>
#include <exception>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "completion_signatures.hpp"
#include "env.hpp"
#include "sender.hpp"
#include "stop_token.hpp"
#include "type_list.hpp"

namespace flow::execution {

// [exec.when_any], when_any combinator - completes with first result

// Minimal aggregate tuple for immovable operation states
// Based on stdexec's __tuple - stores elements as direct members for aggregate initialization
// Supports unlimited number of elements through recursive structure
namespace _when_any_detail {

// Index sequence utilities for compile-time indexing
template <std::size_t... Is>
struct _indices {};

template <std::size_t N, std::size_t... Is>
struct _make_indices : _make_indices<N - 1, N - 1, Is...> {};

template <std::size_t... Is>
struct _make_indices<0, Is...> {
  using type = _indices<Is...>;
};

template <std::size_t N>
using _make_indices_t = typename _make_indices<N>::type;

// Forward declaration
template <class... Ts>
struct _op_tuple;

// Empty base case
template <>
struct _op_tuple<> {};

// Single element specialization
template <class T>
struct _op_tuple<T> {
  T _head;
};

// Recursive case: stores first element and rest in nested tuple
template <class T, class U, class... Ts>
struct _op_tuple<T, U, Ts...> {
  T                   _head;
  _op_tuple<U, Ts...> _tail;
};

// Element access by index - base cases
template <class T>
constexpr T& _get_impl(_op_tuple<T>& t,
                       std::integral_constant<std::size_t, 0> /*unused*/) noexcept {
  return t._head;
}

template <class T>
constexpr const T& _get_impl(const _op_tuple<T>& t,
                             std::integral_constant<std::size_t, 0> /*unused*/) noexcept {
  return t._head;
}

// Recursive cases for multi-element tuples
template <class T, class U, class... Ts>
constexpr T& _get_impl(_op_tuple<T, U, Ts...>& t,
                       std::integral_constant<std::size_t, 0> /*unused*/) noexcept {
  return t._head;
}

template <class T, class U, class... Ts>
constexpr const T& _get_impl(const _op_tuple<T, U, Ts...>& t,
                             std::integral_constant<std::size_t, 0> /*unused*/) noexcept {
  return t._head;
}

template <std::size_t I, class T, class U, class... Ts>
constexpr auto& _get_impl(_op_tuple<T, U, Ts...>& t,
                          std::integral_constant<std::size_t, I> /*unused*/) noexcept {
  return _get_impl(t._tail, std::integral_constant<std::size_t, I - 1>{});
}

template <std::size_t I, class T, class U, class... Ts>
constexpr const auto& _get_impl(const _op_tuple<T, U, Ts...>& t,
                                std::integral_constant<std::size_t, I> /*unused*/) noexcept {
  return _get_impl(t._tail, std::integral_constant<std::size_t, I - 1>{});
}

// Public get interface
template <std::size_t I, class... Ts>
constexpr auto& get(_op_tuple<Ts...>& t) noexcept {
  return _get_impl(t, std::integral_constant<std::size_t, I>{});
}

template <std::size_t I, class... Ts>
constexpr const auto& get(const _op_tuple<Ts...>& t) noexcept {
  return _get_impl(t, std::integral_constant<std::size_t, I>{});
}

// Extract value types from a sender's value_types
template <class S>
struct _sender_value_types {
  using type = typename S::value_types;
};

template <class S>
using sender_value_types_t = typename _sender_value_types<S>::type;

// Trait to check if a type is tuple-like (std::tuple, std::pair, etc.)
template <class T>
struct _is_tuple_like_impl {
 private:
  template <class U>
  static auto test(int) -> decltype(std::tuple_size_v<std::remove_cvref_t<U>>, std::true_type{});

  template <class>
  static std::false_type test(...);

 public:
  using type                  = decltype(test<T>(0));
  static constexpr bool value = type::value;
};

template <class T>
inline constexpr bool is_tuple_like_v = _is_tuple_like_impl<T>::value;

// Convert type_list to variant
template <class... Ts>
struct _type_list_to_variant;

template <class... Ts>
struct _type_list_to_variant<type_list<Ts...>> {
  using type = std::variant<Ts...>;
};

template <>
struct _type_list_to_variant<type_list<>> {
  using type = std::variant<std::tuple<>>;
};

template <class T>
using type_list_to_variant_t = typename _type_list_to_variant<T>::type;

// Concatenate type_lists
template <class... Lists>
struct _concat_type_lists;

template <>
struct _concat_type_lists<> {
  using type = type_list<>;
};

template <class... Ts>
struct _concat_type_lists<type_list<Ts...>> {
  using type = type_list<Ts...>;
};

template <class... Ts, class... Us, class... Rest>
struct _concat_type_lists<type_list<Ts...>, type_list<Us...>, Rest...> {
  using type = typename _concat_type_lists<type_list<Ts..., Us...>, Rest...>::type;
};

template <class... Lists>
using concat_type_lists_t = typename _concat_type_lists<Lists...>::type;

// Deduplicate types in a type_list
template <class TypeList>
struct _unique_types;

template <>
struct _unique_types<type_list<>> {
  using type = type_list<>;
};

template <class T>
struct _unique_types<type_list<T>> {
  using type = type_list<T>;
};

template <class T, class... Ts>
struct _unique_types<type_list<T, Ts...>> {
  template <class U>
  struct _contains;

  template <class... Us>
  struct _contains<type_list<Us...>> {
    static constexpr bool value = (std::is_same_v<T, Us> || ...);
  };

  using rest = typename _unique_types<type_list<Ts...>>::type;
  using type = std::conditional_t<_contains<rest>::value, rest,
                                  typename _concat_type_lists<type_list<T>, rest>::type>;
};

template <class TypeList>
using unique_types_t = typename _unique_types<TypeList>::type;

// Get all value types from all senders (union of all value types)
template <class... Sndrs>
using all_value_types_t = unique_types_t<concat_type_lists_t<sender_value_types_t<Sndrs>...>>;

// Helper to convert type_list<Args...> to completion_signatures<set_value_t(Args)...>
// For when value types are already unwrapped (not tuples)
template <class TypeList>
struct _make_value_completion_signatures;

template <class... Args>
struct _make_value_completion_signatures<type_list<Args...>> {
  using type =
      completion_signatures<set_value_t(Args)..., set_error_t(std::exception_ptr), set_stopped_t()>;
};

template <class TypeList>
auto make_value_completion_signatures() {
  return typename _make_value_completion_signatures<TypeList>::type{};
}

// Helper to convert type_list<tuple<Args1...>, tuple<Args2...>, ...>
// to completion_signatures<set_value_t(Args1...), set_value_t(Args2...), ...>
template <class TypeList>
struct _make_completion_signatures_from_types;

template <class... Tuples>
struct _make_completion_signatures_from_types<type_list<Tuples...>> {
  template <class Tuple>
  struct _tuple_to_sig;

  template <class... Args>
  struct _tuple_to_sig<std::tuple<Args...>> {
    using type = set_value_t(Args...);
  };

  using type = completion_signatures<typename _tuple_to_sig<Tuples>::type...,
                                     set_error_t(std::exception_ptr), set_stopped_t()>;
};

template <class TypeList>
auto make_completion_signatures_from_types() {
  return typename _make_completion_signatures_from_types<TypeList>::type{};
}

}  // namespace _when_any_detail

template <sender... Sndrs>
struct _when_any_sender {
  using sender_concept = sender_t;

  // Helper to wrap value_types in tuples
  template <class TypeList>
  struct _wrap_in_tuple;

  template <>
  struct _wrap_in_tuple<type_list<>> {
    using type = std::tuple<>;  // Void sender: use empty tuple
  };

  template <class... Ts>
  struct _wrap_in_tuple<type_list<Ts...>> {
    using type = std::tuple<Ts...>;  // Wrap all values in tuple
  };

  template <class... Ss>
  struct _get_value_tuples {
    // Get the value_types from each sender and wrap in tuples
    // This gives us variant<tuple<int>, tuple<double>> for consistency
    using type = type_list<
        typename _wrap_in_tuple<typename _when_any_detail::_sender_value_types<Ss>::type>::type...>;
  };

  // Helper to detect if all types are the same (homogeneous)
  template <class TypeList>
  struct _is_homogeneous;

  template <class T>
  struct _is_homogeneous<type_list<T>> : std::true_type {};

  template <class T, class... Ts>
  struct _is_homogeneous<type_list<T, Ts...>> : std::bool_constant<(std::same_as<T, Ts> && ...)> {};

  // Helper to compute public value_types from value_tuples
  template <class ValueTuples>
  struct _compute_value_types;

  // Single tuple case (homogeneous single sender): unwrap the tuple
  template <class... Ts>
  struct _compute_value_types<type_list<std::tuple<Ts...>>> {
    using type = type_list<Ts...>;
  };

  // Homogeneous case with multiple identical tuples: unwrap one
  template <class Tuple, class... Rest>
    requires(_is_homogeneous<type_list<Tuple, Rest...>>::value && sizeof...(Rest) > 0)
  struct _compute_value_types<type_list<Tuple, Rest...>> {
    using type = typename _compute_value_types<type_list<Tuple>>::type;
  };

  // Heterogeneous case: return variant wrapped in type_list
  template <class... Tuples>
    requires(!_is_homogeneous<type_list<Tuples...>>::value && sizeof...(Tuples) > 1)
  struct _compute_value_types<type_list<Tuples...>> {
    using type =
        type_list<typename _when_any_detail::_type_list_to_variant<type_list<Tuples...>>::type>;
  };

  // Union of all possible value types from all senders - deduplicated
  using value_tuples = typename _get_value_tuples<Sndrs...>::type;

  // Public value_types for compatibility with adaptors
  using value_types = typename _compute_value_types<value_tuples>::type;

  // Variant of tuples for internal storage
  using result_variant_t = typename _when_any_detail::_type_list_to_variant<value_tuples>::type;

  // Result type: unwrapped value for homogeneous, variant for heterogeneous
  using result_type =
      std::conditional_t<_is_homogeneous<value_tuples>::value,
                         typename _when_any_detail::_type_list_to_variant<
                             value_tuples>::type,  // Will be variant<T> for homogeneous
                         result_variant_t>;

  std::tuple<Sndrs...> senders_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // For homogeneous when_any: returns the values directly (unwrapped from tuple)
    // For heterogeneous when_any: returns variant for pattern matching
    if constexpr (_is_homogeneous<value_tuples>::value) {
      // Homogeneous case: extract the single tuple type and unwrap it
      using single_tuple = std::variant_alternative_t<0, result_variant_t>;
      // Convert tuple<Ts...> to set_value_t(Ts...)
      return []<class... Ts>(std::tuple<Ts...>*) {
        return completion_signatures<set_value_t(Ts...), set_error_t(std::exception_ptr),
                                     set_stopped_t()>{};
      }(static_cast<single_tuple*>(nullptr));
    } else {
      // Heterogeneous case: return variant for pattern matching
      return completion_signatures<set_value_t(result_variant_t), set_error_t(std::exception_ptr),
                                   set_stopped_t()>{};
    }
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _when_any_operation<R, Sndrs...>{std::move(senders_), std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _when_any_operation<R, Sndrs...>{senders_, std::forward<R>(r)};
  }

 private:
  template <class Rcvr, class... Ss>
  struct _when_any_operation {
    using operation_state_concept = operation_state_t;

    // Stop callback helper
    struct on_stop_requested {
      inplace_stop_source&
           stop_source_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
      void operator()() noexcept {
        stop_source_.request_stop();
      }
    };

    // Forward declare inner_receiver to break circular dependency
    template <std::size_t I>
    struct _inner_receiver;

    // Unified result storage - stores completion tag + args in tuples
    using result_tuple_t =
        std::variant<std::tuple<set_value_t, result_variant_t>,    // Value completion
                     std::tuple<set_error_t, std::exception_ptr>,  // Error completion
                     std::tuple<set_stopped_t>                     // Stopped completion
                     >;

    // Stop callback type for external stop token (use outer Rcvr, not inner receiver)
    using outer_receiver_env_t = decltype(get_env(std::declval<const Rcvr&>()));
    using outer_stop_token_t   = stop_token_of_t<outer_receiver_env_t>;
    using on_stop_callback_t   = stop_callback_for_t<outer_stop_token_t, on_stop_requested>;

    // Members must be declared before op_tuple_t computation
    Rcvr                              receiver_;
    inplace_stop_source               stop_source_;
    std::atomic<bool>                 completed_{false};
    std::atomic<std::size_t>          remaining_{sizeof...(Ss)};
    std::optional<result_tuple_t>     result_;
    std::optional<on_stop_callback_t> on_stop_callback_;

    // Now compute op_state types (this may trigger inner_receiver instantiation)
    template <std::size_t I>
    using op_state_t =
        decltype(std::declval<std::tuple_element_t<I, std::tuple<Ss...>>&&>().connect(
            std::declval<_inner_receiver<I>>()));

    // Recursive type builder for nested op_tuple structure
    // The type is simply _op_tuple<op_state_t<0>, op_state_t<1>, ...>
    template <std::size_t... Is>
    static auto make_op_tuple_type(std::index_sequence<Is...>)
        -> _when_any_detail::_op_tuple<op_state_t<Is>...>;

    using op_tuple_t = decltype(make_op_tuple_type(std::make_index_sequence<sizeof...(Ss)>{}));

    op_tuple_t ops_;

    // Aggregate initialization works with immovable types!
    template <std::size_t... Is>
    static auto connect_senders_helper(std::tuple<Ss...>&& sndrs, _when_any_operation* self,
                                       std::index_sequence<Is...> /*unused*/) -> op_tuple_t {
      return op_tuple_t{std::get<Is>(std::move(sndrs)).connect(_inner_receiver<Is>{self})...};
    }

    _when_any_operation(std::tuple<Ss...>&& sndrs, Rcvr&& r)
        : receiver_(std::move(r)),
          ops_(connect_senders_helper(std::move(sndrs), this,
                                      std::make_index_sequence<sizeof...(Ss)>{})) {}

    void start() & noexcept {
      constexpr std::size_t N = sizeof...(Ss);
      if constexpr (N == 0) {
        std::move(receiver_).set_stopped();
        return;
      }

      // Register external stop callback before starting operations
      // This allows external cancellation to propagate to our internal stop_source
      on_stop_callback_.emplace(get_stop_token(get_env(receiver_)),
                                on_stop_requested{stop_source_});

      // Check if already stopped (external or internal)
      if (stop_source_.stop_requested()) {
        std::move(receiver_).set_stopped();
        return;
      }

      start_all(std::make_index_sequence<N>{});
    }

    template <std::size_t... Is>
    void start_all(std::index_sequence<Is...> /*unused*/) noexcept {
      (..., _when_any_detail::get<Is>(ops_).start());
    }

    void complete_with_result() {
      // Unregister external stop callback before completing
      on_stop_callback_.reset();

      if (!result_.has_value()) {
        // Should not happen - defensive programming
        std::move(receiver_).set_stopped();
        return;
      }

      // Check if externally stopped
      auto stop_token = get_stop_token(get_env(receiver_));
      if (stop_token.stop_requested()) {
        std::move(receiver_).set_stopped();
        return;
      }

      // Visit the result and forward to receiver
      std::visit(
          [this](auto&& tuple) {
            std::apply(
                [this](auto tag, auto&&... args) {
                  // Forward completion to receiver
                  if constexpr (std::same_as<decltype(tag), set_value_t>) {
                    // For homogeneous: unwrap and forward values
                    // For heterogeneous: forward variant
                    if constexpr (_is_homogeneous<value_tuples>::value) {
                      // args is a single argument: the result_variant_t
                      // Extract first alternative (all are the same)
                      auto forward_values = [this](auto&& variant) {
                        auto tup = std::get<0>(std::forward<decltype(variant)>(variant));
                        std::apply(
                            [this](auto&&... vals) {
                              std::move(receiver_).set_value(std::forward<decltype(vals)>(vals)...);
                            },
                            std::move(tup));
                      };
                      forward_values(std::forward<decltype(args)>(args)...);
                    } else {
                      std::move(receiver_).set_value(std::forward<decltype(args)>(args)...);
                    }
                  } else if constexpr (std::same_as<decltype(tag), set_error_t>) {
                    std::move(receiver_).set_error(std::forward<decltype(args)>(args)...);
                  } else {
                    std::move(receiver_).set_stopped();
                  }
                },
                std::forward<decltype(tuple)>(tuple));
          },
          std::move(*result_));
    }

    // Helper for nothrow check
    template <class... Args>
    static constexpr bool all_nothrow_decay_copyable_v =
        (std::is_nothrow_move_constructible_v<std::decay_t<Args>> && ...);

    template <std::size_t I>
    struct _inner_receiver {
      using receiver_concept = receiver_t;

      _when_any_operation* parent_;

      template <class... Args>
      void set_value(Args&&... args) && noexcept {
        bool expected = false;
        if (parent_->completed_.compare_exchange_strong(expected, true,
                                                        std::memory_order_acq_rel)) {
          // First to complete wins
          // Request stop on remaining operations
          parent_->stop_source_.request_stop();

          // Store result using nothrow optimization
          if constexpr (all_nothrow_decay_copyable_v<Args...>) {
            // Nothrow path - direct emplacement
            result_variant_t value;
            if constexpr (_is_homogeneous<value_tuples>::value) {
              value.template emplace<0>(std::forward<Args>(args)...);
            } else {
              value.template emplace<I>(std::forward<Args>(args)...);
            }
            parent_->result_.emplace(std::in_place_index<0>, set_value_t{}, std::move(value));
          } else {
            // May throw - use try-catch
            try {
              result_variant_t value;
              if constexpr (_is_homogeneous<value_tuples>::value) {
                value.template emplace<0>(std::forward<Args>(args)...);
              } else {
                value.template emplace<I>(std::forward<Args>(args)...);
              }
              parent_->result_.emplace(std::in_place_index<0>, set_value_t{}, std::move(value));
            } catch (...) {
              parent_->result_.emplace(std::in_place_index<1>, set_error_t{},
                                       std::current_exception());
            }
          }
        }

        if (parent_->remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          parent_->complete_with_result();
        }
      }

      template <class E>
      void set_error(E&& e) && noexcept {
        bool expected = false;
        if (parent_->completed_.compare_exchange_strong(expected, true,
                                                        std::memory_order_acq_rel)) {
          // Request stop on remaining operations
          parent_->stop_source_.request_stop();

          // Store error
          try {
            std::exception_ptr ep;
            if constexpr (std::same_as<std::decay_t<E>, std::exception_ptr>) {
              ep = std::forward<E>(e);
            } else {
              ep = std::make_exception_ptr(std::forward<E>(e));
            }
            parent_->result_.emplace(std::in_place_index<1>, set_error_t{}, std::move(ep));
          } catch (...) {
            parent_->result_.emplace(std::in_place_index<1>, set_error_t{},
                                     std::current_exception());
          }
        }

        if (parent_->remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          parent_->complete_with_result();
        }
      }

      void set_stopped() && noexcept {
        // Per P2300R11: set_stopped is a first-to-complete winner
        bool expected = false;
        if (parent_->completed_.compare_exchange_strong(expected, true,
                                                        std::memory_order_acq_rel)) {
          // First to complete wins - including stopped
          // Request stop on remaining operations
          parent_->stop_source_.request_stop();

          // Store stopped result
          parent_->result_.emplace(std::in_place_index<2>, set_stopped_t{});
        }

        // Decrement remaining count and complete if last
        if (parent_->remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          parent_->complete_with_result();
        }
      }

      auto get_env() const noexcept {
        // Inject stop token into environment and forward parent receiver's environment
        // This ensures all environment queries are properly forwarded to nested operations
        return make_env_with_stop_token(parent_->stop_source_.get_token(),
                                        flow::execution::get_env(parent_->receiver_));
      }
    };
  };
};

struct when_any_t {
  template <sender... Sndrs>
    requires(sizeof...(Sndrs) > 0)
  constexpr auto operator()(Sndrs&&... sndrs) const {
    return _when_any_sender<__decay_t<Sndrs>...>{std::tuple{std::forward<Sndrs>(sndrs)...}};
  }
};

inline constexpr when_any_t when_any{};

}  // namespace flow::execution
