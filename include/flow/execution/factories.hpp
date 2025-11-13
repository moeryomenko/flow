#pragma once

#include <tuple>
#include <utility>

#include "completion_signatures.hpp"
#include "sender.hpp"
#include "type_list.hpp"

namespace flow::execution {

// Forward declarations
template <class... Vs>
struct _just_sender;

template <class E>
struct _just_error_sender;

struct _just_stopped_sender;

// [exec.factories.just], just sender
template <class... Vs>
struct _just_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<Vs...>;

  std::tuple<Vs...> values_;

  // Default constructor for empty parameter pack
  _just_sender()
    requires(sizeof...(Vs) == 0)
  = default;

  // Allow implicit copy/move constructors
  _just_sender(const _just_sender&)            = default;
  _just_sender(_just_sender&&)                 = default;
  _just_sender& operator=(const _just_sender&) = default;
  _just_sender& operator=(_just_sender&&)      = default;

  template <class... Ts>
    requires(sizeof...(Ts) == sizeof...(Vs) && sizeof...(Ts) > 0)
  explicit _just_sender(Ts&&... ts) : values_(std::forward<Ts>(ts)...) {}

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const noexcept {
    return completion_signatures<set_value_t(Vs...)>{};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) && {
    return _just_operation<R, Vs...>{std::move(values_), std::forward<R>(r)};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) & {
    return _just_operation<R, Vs...>{values_, std::forward<R>(r)};
  }

 private:
  template <class R, class... Ts>
  struct _just_operation {
    using operation_state_concept = operation_state_t;

    std::tuple<Ts...> values_;
    R                 receiver_;

    _just_operation(std::tuple<Ts...>&& vals, R&& r)
        : values_(std::move(vals)), receiver_(std::move(r)) {}

    _just_operation(const std::tuple<Ts...>& vals, R&& r)
        : values_(vals), receiver_(std::move(r)) {}

    void start() & noexcept {
      std::apply(
          [this](auto&&... args) -> auto {
            std::move(receiver_).set_value(std::forward<decltype(args)>(args)...);
          },
          std::move(values_));
    }
  };
};

struct just_t {
  template <class... Vs>
  constexpr auto operator()(Vs&&... vs) const {
    return _just_sender<__decay_t<Vs>...>{std::forward<Vs>(vs)...};
  }

  auto operator()() const {
    return _just_sender<>{};
  }
};

inline constexpr just_t just{};

// [exec.factories.just_error], just_error sender
template <class E>
struct _just_error_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<>;  // Sends no values, only error

  E error_;

  template <class Err>
    requires(!std::same_as<std::decay_t<Err>, _just_error_sender>)
  explicit _just_error_sender(Err&& e) : error_(std::forward<Err>(e)) {}

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const noexcept {
    return completion_signatures<set_error_t(E)>{};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) && {
    return _just_error_operation<R, E>{std::move(error_), std::forward<R>(r)};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) & {
    return _just_error_operation<R, E>{error_, std::forward<R>(r)};
  }

 private:
  template <class R, class Err>
  struct _just_error_operation {
    using operation_state_concept = operation_state_t;

    Err error_;
    R   receiver_;

    _just_error_operation(Err&& e, R&& r) : error_(std::move(e)), receiver_(std::move(r)) {}

    void start() & noexcept {
      std::move(receiver_).set_error(std::move(error_));
    }
  };
};

struct just_error_t {
  template <class E>
  constexpr auto operator()(E&& e) const {
    return _just_error_sender<__decay_t<E>>{std::forward<E>(e)};
  }
};

inline constexpr just_error_t just_error{};

// [exec.factories.just_stopped], just_stopped sender
struct _just_stopped_sender {
  using sender_concept = sender_t;
  using value_types    = type_list<>;  // Sends no values, only stopped

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const noexcept {
    return completion_signatures<set_stopped_t()>{};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) && {
    return _just_stopped_operation<R>{std::forward<R>(r)};
  }

  template <class R>
    requires receiver<R>
  auto connect(R&& r) & {
    return _just_stopped_operation<R>{std::forward<R>(r)};
  }

 private:
  template <class R>
  struct _just_stopped_operation {
    using operation_state_concept = operation_state_t;

    R receiver_;

    explicit _just_stopped_operation(R&& r) : receiver_(std::move(r)) {}

    void start() & noexcept {
      std::move(receiver_).set_stopped();
    }
  };
};

struct just_stopped_t {
  constexpr auto operator()() const {
    return _just_stopped_sender{};
  }
};

inline constexpr just_stopped_t just_stopped{};

}  // namespace flow::execution
