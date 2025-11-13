#pragma once

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

#include "completion_signatures.hpp"
#include "sender.hpp"
#include "type_list.hpp"

namespace flow::execution {

// Helper to deduce the return type of a function given sender value types
namespace _then_detail {

// Extract type_list elements as parameter pack
template <class TypeList>
struct _type_list_elements;

template <class... Ts>
struct _type_list_elements<type_list<Ts...>> {
  template <class F>
  using invoke_result = std::invoke_result<F, Ts...>;
};

// Deduce the return type of F when called with sender's value types
template <class S, class F>
struct _deduce_then_result {
  using sender_values = typename S::value_types;
  using invoke_result = typename _type_list_elements<sender_values>::template invoke_result<F>;
  using type          = typename invoke_result::type;
};

template <class S, class F>
using deduce_then_result_t = typename _deduce_then_result<S, F>::type;

// Wrap result in type_list, but handle void specially
template <class T>
struct _wrap_in_type_list {
  using type = type_list<T>;
};

template <>
struct _wrap_in_type_list<void> {
  using type = type_list<>;  // Empty type_list for void
};

template <class T>
using wrap_in_type_list_t = typename _wrap_in_type_list<T>::type;

// Helper to convert type_list to set_value_t signature
template <class TypeList>
struct _type_list_to_set_value;

template <class... Ts>
struct _type_list_to_set_value<type_list<Ts...>> {
  using type = set_value_t(Ts...);
};

template <class TypeList>
using type_list_to_set_value_t = typename _type_list_to_set_value<TypeList>::type;

}  // namespace _then_detail

// [exec.adaptors.then], then adaptor
template <sender S, class F>
struct _then_sender {
  using sender_concept = sender_t;
  // Deduce value type from function result based on sender's value types
  using value_types = _then_detail::wrap_in_type_list_t<_then_detail::deduce_then_result_t<S, F>>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Convert value_types (type_list) to proper set_value_t signature
    using set_value_sig = _then_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(_then_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_then_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _then_receiver {
    using receiver_concept = receiver_t;

    Fn   fun_;
    Rcvr receiver_;

    void set_value() && noexcept {
      try {
        if constexpr (std::is_void_v<std::invoke_result_t<Fn>>) {
          std::invoke(std::move(fun_));
          std::move(receiver_).set_value();
        } else {
          std::move(receiver_).set_value(std::invoke(std::move(fun_)));
        }
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
          std::invoke(std::move(fun_), std::forward<Args>(args)...);
          std::move(receiver_).set_value();
        } else {
          std::move(receiver_).set_value(std::invoke(std::move(fun_), std::forward<Args>(args)...));
        }
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }

    template <class E>
    void set_error(E&& e) && noexcept {
      std::move(receiver_).set_error(std::forward<E>(e));
    }

    void set_stopped() && noexcept {
      std::move(receiver_).set_stopped();
    }
  };
};

// Forward declaration for pipeable support
template <class F>
struct _pipeable_then;

// Implementation of then function object
struct then_t {
  // Direct call with sender and function
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _then_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  // Curried call for pipe syntax
  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_then<__decay_t<F>>{std::forward<F>(f)};
  }
};

inline constexpr then_t then{};

// Pipeable struct implementation
template <class F>
struct _pipeable_then {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_then& p) {
    return then_t{}(std::forward<S>(s), p.fun_);
  }
};

}  // namespace flow::execution
