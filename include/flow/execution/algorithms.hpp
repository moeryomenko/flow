#pragma once

#include <atomic>
#include <exception>
#include <mutex>
#include <optional>
#include <tuple>
#include <utility>

#include "completion_signatures.hpp"
#include "sender.hpp"
#include "type_list.hpp"

namespace flow::execution {

// [exec.bulk], bulk execution
template <sender S, class Shape, class F>
struct _bulk_sender {
  using sender_concept = sender_t;
  // bulk forwards the same values as the underlying sender
  using value_types = typename S::value_types;  // Forward from underlying sender

  S     sender_;
  Shape shape_;
  F     fun_;

  template <class Env>
  auto get_completion_signatures(Env&& /*env*/) const {
    return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return std::move(sender_).connect(
        _bulk_receiver<Shape, F, R>{shape_, std::move(fun_), std::forward<R>(r)});
  }

  template <receiver R>
  auto connect(R&& r) & {
    return sender_.connect(_bulk_receiver<Shape, F, R>{shape_, fun_, std::forward<R>(r)});
  }

 private:
  template <class Sh, class Fn, class Rcvr>
  struct _bulk_receiver {
    using receiver_concept = receiver_t;

    Sh   shape_;
    Fn   fun_;
    Rcvr receiver_;

    template <class... Args>
    void set_value(Args&&... args) && noexcept {
      try {
        for (Sh i = 0; i < shape_; ++i) {
          fun_(i, std::forward<Args>(args)...);
        }
        std::move(receiver_).set_value(std::forward<Args>(args)...);
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

// Pipeable version forward declaration
template <class Shape, class F>
struct _pipeable_bulk;

struct bulk_t {
  template <sender S, class Shape, class F>
  constexpr auto operator()(S&& s, Shape shape, F&& f) const {
    return _bulk_sender<__decay_t<S>, Shape, __decay_t<F>>{std::forward<S>(s), shape,
                                                           std::forward<F>(f)};
  }

  // Curried version for pipe syntax
  template <class Shape, class F>
  constexpr auto operator()(Shape shape, F&& f) const {
    return _pipeable_bulk<Shape, __decay_t<F>>{shape, std::forward<F>(f)};
  }
};

inline constexpr bulk_t bulk{};

// [exec.when_all], when_all combinator with value aggregation
namespace _when_all_detail {

// Extract first type from type_list
template <class T>
struct _first_type;

template <class T, class... Ts>
struct _first_type<type_list<T, Ts...>> {
  using type = T;
};

template <>
struct _first_type<type_list<>> {
  using type = void;
};

template <class T>
using first_type_t = typename _first_type<T>::type;

// Extract value type from sender (using their value_types which is now type_list)
template <class S>
struct _sender_value_type {
  using type = first_type_t<typename S::value_types>;
};

template <class S>
using sender_value_type_t = typename _sender_value_type<S>::type;

// Helper to store a single value from a sender
template <class T>
struct _value_holder {
  std::optional<T> value;

  template <class... Args>
  void store(Args&&... args) {
    value.emplace(std::forward<Args>(args)...);
  }

  T&& get() && {
    return std::move(*value);
  }
};

// Specialization for void (no value)
template <>
struct _value_holder<void> {
  void store() {}
  void get() && {}
};

}  // namespace _when_all_detail

template <sender... Sndrs>
struct _when_all_sender {
  using sender_concept = sender_t;
  // value_types is now a type_list instead of std::tuple
  using value_types = type_list<_when_all_detail::sender_value_type_t<Sndrs>...>;

  std::tuple<Sndrs...> senders_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Sends values directly as separate arguments, not as a tuple
    return completion_signatures<set_value_t(_when_all_detail::sender_value_type_t<Sndrs>...),
                                 set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _when_all_operation<R, Sndrs...>{std::move(senders_), std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _when_all_operation<R, Sndrs...>{senders_, std::forward<R>(r)};
  }

 private:
  template <class Rcvr, class... Ss>
  struct _when_all_operation {
    using operation_state_concept = operation_state_t;

    // Create tuple of value holders - one per sender, each with correct type
    using values_type =
        std::tuple<_when_all_detail::_value_holder<_when_all_detail::sender_value_type_t<Ss>>...>;

    std::tuple<Ss...>        senders_;
    Rcvr                     receiver_;
    std::atomic<std::size_t> completed_{0};
    std::atomic<bool>        error_occurred_{false};
    values_type              values_;
    std::mutex               mutex_;
    _when_all_operation(std::tuple<Ss...>&& sndrs, Rcvr&& r)
        : senders_(std::move(sndrs)), receiver_(std::move(r)) {}

    void start() & noexcept {
      constexpr std::size_t N = sizeof...(Ss);
      if constexpr (N == 0) {
        std::move(receiver_).set_value();
        return;
      }
      start_impl(std::make_index_sequence<N>{});
    }

    template <std::size_t... Is>
    void start_impl(std::index_sequence<Is...> /*unused*/) {
      (start_one<Is>(std::get<Is>(std::move(senders_))), ...);
    }

    template <std::size_t I, class S>
    void start_one(S&& sndr) {
      auto inner_receiver = _inner_receiver<I>{this};
      auto op             = std::forward<S>(sndr).connect(std::move(inner_receiver));
      op.start();
    }

    template <std::size_t I>
    struct _inner_receiver {
      using receiver_concept = receiver_t;

      _when_all_operation* parent_;

      template <class... Args>
      void set_value(Args&&... args) && noexcept {
        constexpr std::size_t N = sizeof...(Ss);

        // Store the value
        {
          std::scoped_lock lock(parent_->mutex_);
          if constexpr (sizeof...(Args) > 0) {
            std::get<I>(parent_->values_).store(std::forward<Args>(args)...);
          }
        }

        auto count = parent_->completed_.fetch_add(1, std::memory_order_acq_rel) + 1;

        if (count == N && !parent_->error_occurred_.load(std::memory_order_acquire)) {
          // All completed - send aggregated values
          parent_->send_values(std::make_index_sequence<N>{});
        }
      }

      template <class E>
      void set_error(E&& e) && noexcept {
        bool expected = false;
        if (parent_->error_occurred_.compare_exchange_strong(expected, true,
                                                             std::memory_order_acq_rel)) {
          std::move(parent_->receiver_).set_error(std::forward<E>(e));
        }
      }

      void set_stopped() && noexcept {
        bool expected = false;
        if (parent_->error_occurred_.compare_exchange_strong(expected, true,
                                                             std::memory_order_acq_rel)) {
          std::move(parent_->receiver_).set_stopped();
        }
      }
    };

    template <std::size_t... Is>
    void send_values(std::index_sequence<Is...> /*unused*/) {
      std::move(receiver_).set_value(std::move(std::get<Is>(values_)).get()...);
    }
  };
};

struct when_all_t {
  template <sender... Sndrs>
  constexpr auto operator()(Sndrs&&... sndrs) const {
    return _when_all_sender<__decay_t<Sndrs>...>{std::tuple{std::forward<Sndrs>(sndrs)...}};
  }
};

inline constexpr when_all_t when_all{};

// Pipeable versions
template <class Shape, class F>
struct _pipeable_bulk {
  Shape shape_;
  F     fun_;

  template <sender S>
  friend auto operator|(S&& s, const _pipeable_bulk& p) {
    return bulk_t{}(std::forward<S>(s), p.shape_, p.fun_);
  }
};

}  // namespace flow::execution
