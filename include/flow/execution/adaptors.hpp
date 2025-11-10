#pragma once

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#include "execution.hpp"
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
  auto get_completion_signatures(Env&& env) const {
    // Simplified: assumes F returns a value or throws
    using result_t = std::invoke_result_t<F>;
    return completion_signatures<set_value_t(result_t), set_error_t(std::exception_ptr),
                                 set_stopped_t()>{};
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

// Helper for upon_error return type deduction
namespace _upon_error_detail {

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

}  // namespace _upon_error_detail

// [exec.adaptors.upon_error], upon_error adaptor
template <sender S, class F>
struct _upon_error_sender {
  using sender_concept = sender_t;
  // upon_error converts error to value - deduce from function return type
  using value_types =
      _upon_error_detail::wrap_result_t<_upon_error_detail::deduce_upon_error_result_t<F>>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
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
  using value_types = _upon_error_detail::wrap_result_t<std::invoke_result_t<F>>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
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

// Helper for let_value return type deduction
namespace _let_value_detail {

// Extract the value_types from the sender returned by F
template <class S, class F>
struct _deduce_let_value_result {
  using sender_values = typename S::value_types;
  // F is invoked with the unpacked values from sender_values
  // and returns a new sender
  template <class... Ts>
  static auto invoke_with(type_list<Ts...>) -> std::invoke_result_t<F, Ts...>;

  using result_sender = decltype(invoke_with(sender_values{}));
  using type          = typename result_sender::value_types;
};

template <class S, class F>
using deduce_let_value_result_t = typename _deduce_let_value_result<S, F>::type;

}  // namespace _let_value_detail

// [exec.adaptors.let_value], let_value adaptor
template <sender S, class F>
struct _let_value_sender {
  using sender_concept = sender_t;
  // let_value returns a sender - extract its value_types
  using value_types = _let_value_detail::deduce_let_value_result_t<S, F>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // The completion signatures depend on what sender F returns
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _let_value_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_let_value_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _let_value_receiver {
    using receiver_concept = receiver_t;

    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        auto next_sender = std::invoke(std::move(fun_), std::forward<Args>(args)...);
        auto op          = std::move(next_sender).connect(std::move(receiver_));
        op.start();
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

// [exec.adaptors.let_error], let_error adaptor
template <sender S, class F>
struct _let_error_sender {
  using sender_concept = sender_t;
  // let_error returns a sender - extract its value_types
  // F takes an exception_ptr and returns a sender
  using value_types = typename std::invoke_result_t<F, std::exception_ptr>::value_types;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _let_error_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_let_error_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _let_error_receiver {
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
        auto next_sender = std::invoke(std::move(fun_), std::forward<E>(e));
        auto op          = std::move(next_sender).connect(std::move(receiver_));
        op.start();
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }

    void set_stopped() && noexcept {
      std::move(receiver_).set_stopped();
    }
  };
};

// [exec.adaptors.let_stopped], let_stopped adaptor
template <sender S, class F>
struct _let_stopped_sender {
  using sender_concept = sender_t;
  // let_stopped returns a sender - extract its value_types
  // F takes no arguments and returns a sender
  using value_types = typename std::invoke_result_t<F>::value_types;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _let_stopped_receiver<F, R>{std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_let_stopped_receiver<F, R>{fun_, std::forward<R>(r)});
  }

 private:
  template <class Fn, class Rcvr>
  struct _let_stopped_receiver {
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
        auto next_sender = std::invoke(std::move(fun_));
        auto op          = std::move(next_sender).connect(std::move(receiver_));
        op.start();
      } catch (...) {
        std::move(receiver_).set_error(std::current_exception());
      }
    }
  };
};

// Forward declarations for pipeable support
template <class F>
struct _pipeable_then;

template <class F>
struct _pipeable_upon_error;

template <class F>
struct _pipeable_upon_stopped;

template <class F>
struct _pipeable_let_value;

template <class F>
struct _pipeable_let_error;

template <class F>
struct _pipeable_let_stopped;

// Implementation of adaptor function objects
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

struct let_value_t {
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _let_value_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_let_value<__decay_t<F>>{std::forward<F>(f)};
  }
};

struct let_error_t {
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _let_error_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_let_error<__decay_t<F>>{std::forward<F>(f)};
  }
};

struct let_stopped_t {
  template <sender S, class F>
  constexpr auto operator()(S&& s, F&& f) const {
    return _let_stopped_sender<__decay_t<S>, __decay_t<F>>{std::forward<S>(s), std::forward<F>(f)};
  }

  template <class F>
  constexpr auto operator()(F&& f) const {
    return _pipeable_let_stopped<__decay_t<F>>{std::forward<F>(f)};
  }
};

inline constexpr then_t         then{};
inline constexpr upon_error_t   upon_error{};
inline constexpr upon_stopped_t upon_stopped{};
inline constexpr let_value_t    let_value{};
inline constexpr let_error_t    let_error{};
inline constexpr let_stopped_t  let_stopped{};

// Pipeable struct implementations (after function object definitions)
template <class F>
struct _pipeable_then {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_then& p) {
    return then_t{}(std::forward<S>(s), p.fun_);
  }
};

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

template <class F>
struct _pipeable_let_value {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_let_value& p) {
    return let_value_t{}(std::forward<S>(s), p.fun_);
  }
};

template <class F>
struct _pipeable_let_error {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_let_error& p) {
    return let_error_t{}(std::forward<S>(s), p.fun_);
  }
};

template <class F>
struct _pipeable_let_stopped {
  F fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_let_stopped& p) {
    return let_stopped_t{}(std::forward<S>(s), p.fun_);
  }
};

}  // namespace flow::execution
