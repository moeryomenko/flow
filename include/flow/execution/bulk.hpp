#pragma once

#include <exception>
#include <utility>

#include "execution_policy.hpp"
#include "sender.hpp"

namespace flow::execution {

// [exec.bulk], bulk execution with chunking support

// bulk_chunked: basis operation that processes iterations in chunks
template <sender S, class Policy, class Shape, class F>
struct _bulk_chunked_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S      sender_;
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Forward the completion signatures from the predecessor sender
    return std::move(sender_).get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(_bulk_chunked_receiver<Policy, Shape, F, R>{
        std::move(policy_), shape_, std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(
        _bulk_chunked_receiver<Policy, Shape, F, R>{policy_, shape_, fun_, std::forward<R>(r)});
  }

 private:
  template <class Pol, class Sh, class Fn, class Rcvr>
  struct _bulk_chunked_receiver {
    using receiver_concept = receiver_t;

    Pol  policy_;
    Sh   shape_;
    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        // Call function with begin/end range only if shape > 0
        // Default implementation processes entire range as one chunk
        if (shape_ > 0) {
          if constexpr (sizeof...(Args) > 0) {
            fun_(static_cast<Sh>(0), shape_, args...);
          } else {
            fun_(static_cast<Sh>(0), shape_);
          }
        }
        if constexpr (sizeof...(Args) > 0) {
          std::move(receiver_).set_value(std::forward<Args>(args)...);
        } else {
          std::move(receiver_).set_value();
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

// bulk_unchunked: one execution agent per iteration
template <sender S, class Policy, class Shape, class F>
struct _bulk_unchunked_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S      sender_;
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Forward the completion signatures from the predecessor sender
    return std::move(sender_).get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(_bulk_unchunked_receiver<Policy, Shape, F, R>{
        std::move(policy_), shape_, std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(
        _bulk_unchunked_receiver<Policy, Shape, F, R>{policy_, shape_, fun_, std::forward<R>(r)});
  }

 private:
  template <class Pol, class Sh, class Fn, class Rcvr>
  struct _bulk_unchunked_receiver {
    using receiver_concept = receiver_t;

    Pol  policy_;
    Sh   shape_;
    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        // Call function for each iteration
        for (Sh i = 0; i < shape_; ++i) {
          if constexpr (sizeof...(Args) > 0) {
            fun_(i, args...);
          } else {
            fun_(i);
          }
        }
        if constexpr (sizeof...(Args) > 0) {
          std::move(receiver_).set_value(std::forward<Args>(args)...);
        } else {
          std::move(receiver_).set_value();
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

// bulk: implemented in terms of bulk_chunked
template <sender S, class Policy, class Shape, class F>
struct _bulk_sender {
  using sender_concept = sender_t;
  using value_types    = typename S::value_types;

  S      sender_;
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <class Env>
  auto get_completion_signatures(Env&& env) const {
    // Forward the completion signatures from the predecessor sender
    return std::move(sender_).get_completion_signatures(std::forward<Env>(env));
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(_bulk_receiver<Policy, Shape, F, R>{
        std::move(policy_), shape_, std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(
        _bulk_receiver<Policy, Shape, F, R>{policy_, shape_, fun_, std::forward<R>(r)});
  }

 private:
  template <class Pol, class Sh, class Fn, class Rcvr>
  struct _bulk_receiver {
    using receiver_concept = receiver_t;

    Pol  policy_;
    Sh   shape_;
    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        // Implement bulk in terms of bulk_chunked by iterating within the chunk
        Sh begin = 0;
        Sh end   = shape_;
        while (begin != end) {
          if constexpr (sizeof...(Args) > 0) {
            fun_(begin++, args...);
          } else {
            fun_(begin++);
          }
        }
        if constexpr (sizeof...(Args) > 0) {
          std::move(receiver_).set_value(std::forward<Args>(args)...);
        } else {
          std::move(receiver_).set_value();
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

// Pipeable version forward declarations
template <class Policy, class Shape, class F>
struct _pipeable_bulk;

template <class Policy, class Shape, class F>
struct _pipeable_bulk_chunked;

template <class Policy, class Shape, class F>
struct _pipeable_bulk_unchunked;

struct bulk_chunked_t {
  template <sender S, class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(S&& s, Policy&& policy, Shape shape, F&& f) const {
    return _bulk_chunked_sender<__decay_t<S>, __decay_t<Policy>, Shape, __decay_t<F>>{
        std::forward<S>(s), std::forward<Policy>(policy), shape, std::forward<F>(f)};
  }

  // Curried version for pipe syntax
  template <class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(Policy&& policy, Shape shape, F&& f) const {
    return _pipeable_bulk_chunked<__decay_t<Policy>, Shape, __decay_t<F>>{
        std::forward<Policy>(policy), shape, std::forward<F>(f)};
  }
};

struct bulk_unchunked_t {
  template <sender S, class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(S&& s, Policy&& policy, Shape shape, F&& f) const {
    return _bulk_unchunked_sender<__decay_t<S>, __decay_t<Policy>, Shape, __decay_t<F>>{
        std::forward<S>(s), std::forward<Policy>(policy), shape, std::forward<F>(f)};
  }

  // Curried version for pipe syntax
  template <class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(Policy&& policy, Shape shape, F&& f) const {
    return _pipeable_bulk_unchunked<__decay_t<Policy>, Shape, __decay_t<F>>{
        std::forward<Policy>(policy), shape, std::forward<F>(f)};
  }
};

struct bulk_t {
  template <sender S, class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(S&& s, Policy&& policy, Shape shape, F&& f) const {
    return _bulk_sender<__decay_t<S>, __decay_t<Policy>, Shape, __decay_t<F>>{
        std::forward<S>(s), std::forward<Policy>(policy), shape, std::forward<F>(f)};
  }

  // Curried version for pipe syntax
  template <class Policy, class Shape, class F>
    requires is_execution_policy_v<Policy>
  constexpr auto operator()(Policy&& policy, Shape shape, F&& f) const {
    return _pipeable_bulk<__decay_t<Policy>, Shape, __decay_t<F>>{std::forward<Policy>(policy),
                                                                  shape, std::forward<F>(f)};
  }
};

inline constexpr bulk_chunked_t   bulk_chunked{};
inline constexpr bulk_unchunked_t bulk_unchunked{};
inline constexpr bulk_t           bulk{};

// Pipeable versions
template <class Policy, class Shape, class F>
struct _pipeable_bulk_chunked {
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_bulk_chunked& p) {
    return bulk_chunked_t{}(std::forward<S>(s), p.policy_, p.shape_, p.fun_);
  }
};

template <class Policy, class Shape, class F>
struct _pipeable_bulk_unchunked {
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_bulk_unchunked& p) {
    return bulk_unchunked_t{}(std::forward<S>(s), p.policy_, p.shape_, p.fun_);
  }
};

template <class Policy, class Shape, class F>
struct _pipeable_bulk {
  Policy policy_;
  Shape  shape_;
  F      fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_bulk& p) {
    return bulk_t{}(std::forward<S>(s), p.policy_, p.shape_, p.fun_);
  }
};

}  // namespace flow::execution
