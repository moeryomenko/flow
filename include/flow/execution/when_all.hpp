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

}  // namespace flow::execution
