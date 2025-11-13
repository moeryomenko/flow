#pragma once

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

#include "completion_signatures.hpp"
#include "sender.hpp"
#include "type_list.hpp"

namespace flow::execution {

// Helper for upon_error and upon_stopped return type deduction
namespace _upon_detail {

template <class F>
struct _deduce_upon_error_result {
  // Assume F takes std::exception_ptr
  using type = std::invoke_result_t<F, std::exception_ptr>;
};

template <class F>
using deduce_upon_error_result_t = typename _deduce_upon_error_result<F>::type;

template <class T>
struct _wrap_result {
  using type = flow::execution::type_list<T>;
};

template <>
struct _wrap_result<void> {
  using type = flow::execution::type_list<>;
};

template <class T>
using wrap_result_t = typename _wrap_result<T>::type;

// Helper to convert type_list to set_value_t signature
template <class TypeList>
struct _type_list_to_set_value;

template <class... Ts>
struct _type_list_to_set_value<type_list<Ts...>> {
  using type = set_value_t(Ts...);
};

template <class TypeList>
using type_list_to_set_value_t = typename _type_list_to_set_value<TypeList>::type;

}  // namespace _upon_detail

// [exec.adaptors.upon_error], upon_error adaptor
template <sender S, class F>
struct _upon_error_sender {
  using sender_concept = sender_t;
  // upon_error converts error to value - deduce from function return type
  using value_types = _upon_detail::wrap_result_t<_upon_detail::deduce_upon_error_result_t<F>>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Convert value_types (type_list) to proper set_value_t signature
    using set_value_sig = _upon_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _upon_error_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_upon_error_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _upon_error_receiver {
    using receiver_concept = receiver_t;

    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      std::move(receiver_).set_value(std::forward<Args>(args)...);
    }

    template <class E>
    void set_error(E&& e) && noexcept {
      try {
        if constexpr (std::is_void_v<std::invoke_result_t<Fn, E>>) {
          std::invoke(std::move(fun_), std::forward<E>(e));
          std::move(receiver_).set_value();
        } else {
          std::move(receiver_).set_value(std::invoke(std::move(fun_), std::forward<E>(e)));
        }
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }

    void set_stopped() && noexcept {
      std::move(receiver_).set_stopped();
    }
  };
};

// [exec.adaptors.upon_stopped], upon_stopped adaptor
template <sender S, class F>
struct _upon_stopped_sender {
  using sender_concept = sender_t;
  // upon_stopped converts stopped to value - deduce from function return type
  using value_types = _upon_detail::wrap_result_t<std::invoke_result_t<F>>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
      return completion_signatures<set_value_t(), set_error_t(std::exception_ptr),
                                   set_stopped_t()>{};
    } else {
      return completion_signatures<set_value_t(std::invoke_result_t<F>),
                                   set_error_t(std::exception_ptr), set_stopped_t()>{};
    }
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _upon_stopped_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_upon_stopped_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _upon_stopped_receiver {
    using receiver_concept = receiver_t;

    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      std::move(receiver_).set_value(std::forward<Args>(args)...);
    }

    template <class E>
    void set_error(E&& e) && noexcept {
      std::move(receiver_).set_error(std::forward<E>(e));
    }

    void set_stopped() && noexcept {
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
  };
};

// Forward declarations for pipeable support
template <class F>
struct _pipeable_upon_error;

template <class F>
struct _pipeable_upon_stopped;

// Implementation of upon function objects
struct upon_error_t {
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _upon_error_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_upon_error<__decay_t<F>>{std::forward<F>(f)};
  }
};

struct upon_stopped_t {
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _upon_stopped_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_upon_stopped<__decay_t<F>>{std::forward<F>(f)};
  }
};

inline constexpr upon_error_t   upon_error{};
inline constexpr upon_stopped_t upon_stopped{};

// Pipeable struct implementations
template <class F>
struct _pipeable_upon_error {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_upon_error& p) {
    return upon_error_t{}(std::forward<S>(s), p.fun_);
  }
};

template <class F>
struct _pipeable_upon_stopped {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_upon_stopped& p) {
    return upon_stopped_t{}(std::forward<S>(s), p.fun_);
  }
};

}  // namespace flow::execution
