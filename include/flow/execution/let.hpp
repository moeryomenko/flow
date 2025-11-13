#pragma once

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

#include "completion_signatures.hpp"
#include "sender.hpp"
#include "type_list.hpp"

namespace flow::execution {

// Helper for let_value return type deduction
namespace _let_detail {

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

// Helper to convert type_list to set_value_t signature
template <class TypeList>
struct _type_list_to_set_value;

template <class... Ts>
struct _type_list_to_set_value<type_list<Ts...>> {
  using type = set_value_t(Ts...);
};

template <class TypeList>
using type_list_to_set_value_t = typename _type_list_to_set_value<TypeList>::type;

}  // namespace _let_detail

// [exec.adaptors.let_value], let_value adaptor
template <sender S, class F>
struct _let_value_sender {
  using sender_concept = sender_t;
  // let_value returns a sender - extract its value_types
  using value_types = _let_detail::deduce_let_value_result_t<S, F>;

  S sender_;
  F fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Convert value_types (type_list) to proper set_value_t signature
    using set_value_sig = _let_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
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
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Convert value_types (type_list) to proper set_value_t signature
    using set_value_sig = _let_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
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
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Convert value_types (type_list) to proper set_value_t signature
    using set_value_sig = _let_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
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
struct _pipeable_let_value;

template <class F>
struct _pipeable_let_error;

template <class F>
struct _pipeable_let_stopped;

// Implementation of let function objects
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

inline constexpr let_value_t   let_value{};
inline constexpr let_error_t   let_error{};
inline constexpr let_stopped_t let_stopped{};

// Pipeable struct implementations
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
