#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <tuple>
#include <variant>

#include "execution.hpp"
#include "type_list.hpp"

namespace flow::this_thread {

namespace _sync_wait_detail {

template <class... Ts>
struct _sync_wait_state {
  std::mutex                                                          mutex;
  std::condition_variable                                             cv;
  bool                                                                completed = false;
  std::variant<std::monostate, std::tuple<Ts...>, std::exception_ptr> result;
};

template <class... Ts>
struct _sync_wait_receiver {
  using receiver_concept = execution::receiver_t;

  _sync_wait_state<Ts...>* state_;

  template <class... Args>
    requires std::constructible_from<std::tuple<Ts...>, Args...>
  void set_value(Args&&... args) && noexcept {
    try {
      std::scoped_lock lock(state_->mutex);
      state_->result.template emplace<1>(std::forward<Args>(args)...);
      state_->completed = true;
    } catch (...) {
      std::scoped_lock lock(state_->mutex);
      state_->result.template emplace<2>(std::current_exception());
      state_->completed = true;
    }
    state_->cv.notify_one();
  }

  void set_value() && noexcept
    requires(sizeof...(Ts) == 0)
  {
    std::scoped_lock lock(state_->mutex);
    state_->result.template emplace<1>();
    state_->completed = true;
    state_->cv.notify_one();
  }

  void set_error(std::exception_ptr ep) && noexcept {
    std::scoped_lock lock(state_->mutex);
    state_->result.template emplace<2>(std::move(ep));
    state_->completed = true;
    state_->cv.notify_one();
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    std::scoped_lock lock(state_->mutex);
    try {
      std::rethrow_exception(std::make_exception_ptr(std::forward<E>(e)));
    } catch (...) {
      state_->result.template emplace<2>(std::current_exception());
    }
    state_->completed = true;
    state_->cv.notify_one();
  }

  void set_stopped() && noexcept {
    std::scoped_lock lock(state_->mutex);
    state_->result.template emplace<0>();
    state_->completed = true;
    state_->cv.notify_one();
  }
};

// Helper template to invoke sync_wait with explicit types
template <class... Ts>
struct _sync_wait_with_types_t {
  template <execution::sender S>
  auto operator()(S&& sndr) const -> std::optional<std::tuple<Ts...>> {
    _sync_wait_state<Ts...> state;
    auto                    rcvr = _sync_wait_receiver<Ts...>{&state};

    auto op = execution::connect(std::forward<S>(sndr), std::move(rcvr));

    op.start();

    {
      std::unique_lock lock(state.mutex);
      state.cv.wait(lock, [&] -> auto { return state.completed; });
    }

    return std::visit(
        [](auto&& result) -> std::optional<std::tuple<Ts...>> {
          using T = std::decay_t<decltype(result)>;

          if constexpr (std::is_same_v<T, std::monostate>) {
            // Stopped
            return std::nullopt;
          } else if constexpr (std::is_same_v<T, std::tuple<Ts...>>) {
            // Success
            return result;
          } else if constexpr (std::is_same_v<T, std::exception_ptr>) {
            // Error
            std::rethrow_exception(result);
          }
          return std::nullopt;
        },
        state.result);
  }
};

}  // namespace _sync_wait_detail

// Helper to deduce value types from a sender
namespace _sender_value_types_detail {

// Convert type_list to tuple
template <class T>
struct _type_list_to_tuple;

template <class... Ts>
struct _type_list_to_tuple<execution::type_list<Ts...>> {
  using type = std::tuple<Ts...>;
};

template <class T>
using type_list_to_tuple_t = typename _type_list_to_tuple<T>::type;

// Helper to extract value types from set_value_t signatures
template <class... Ts>
std::tuple<Ts...> _value_types_from_signature(execution::set_value_t(Ts...));

// Try to deduce from completion signatures if available
template <class S>
concept _has_value_types_member = requires { typename S::value_types; };

// Attempt to extract from sender's stored values (for just_sender)
template <class S>
concept _has_values_member = requires(S s) {
  { s.values_ } -> std::same_as<type_list_to_tuple_t<typename S::value_types>&>;
};

// Default: assume single int for senders we can't introspect
template <class S, class = void>
struct _deduce_value_types {
  using type = std::tuple<int>;
};

// Try to use the sender's value_types if available
template <class S>
  requires _has_value_types_member<S>
struct _deduce_value_types<S, void> {
  // Convert from type_list to tuple
  using type = type_list_to_tuple_t<typename S::value_types>;
};

template <class S>
using deduce_value_types_t = typename _deduce_value_types<std::decay_t<S>>::type;

}  // namespace _sender_value_types_detail

struct sync_wait_t {
  // Automatic type deduction from sender
  template <execution::sender S>
  auto operator()(S&& sndr) const {
    using value_tuple = _sender_value_types_detail::deduce_value_types_t<S>;
    return apply_sync_wait(std::forward<S>(sndr), value_tuple{});
  }

 private:
  template <execution::sender S, class... Ts>
  auto apply_sync_wait(S&& sndr, std::tuple<Ts...> /*unused*/) const {
    return _sync_wait_detail::_sync_wait_with_types_t<Ts...>{}(std::forward<S>(sndr));
  }

 public:
  // Explicit type specification
  template <class... Ts>
  auto operator()() const {
    return _sync_wait_detail::_sync_wait_with_types_t<Ts...>{};
  }
};

inline constexpr sync_wait_t sync_wait{};

// [exec.start_detached], start_detached consumer
namespace _start_detached_detail {

struct _start_detached_receiver {
  using receiver_concept = execution::receiver_t;

  template <class... Args>
  void set_value(Args&&... /*unused*/) && noexcept {
    // Fire and forget - do nothing with the value
  }

  template <class E>
  void set_error(E&& e) && noexcept {
    // In a real implementation, might want to log or report this
    // For now, just terminate since we can't propagate
    std::terminate();
  }

  void set_stopped() && noexcept {
    // Fire and forget - do nothing when stopped
  }
};

}  // namespace _start_detached_detail

struct start_detached_t {
  template <execution::sender S>
  void operator()(S&& sndr) const {
    auto rcvr = _start_detached_detail::_start_detached_receiver{};
    auto op   = execution::connect(std::forward<S>(sndr), std::move(rcvr));
    op.start();
    // Operation completes asynchronously, we don't wait
  }
};

inline constexpr start_detached_t start_detached{};

}  // namespace flow::this_thread
