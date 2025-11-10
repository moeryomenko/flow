#pragma once

#include <concepts>
#include <utility>

#include "env.hpp"
#include "operation_state.hpp"
#include "receiver.hpp"
#include "utils.hpp"

namespace flow::execution {

// Forward declaration for sender_to concept
template <class... Sigs>
struct completion_signatures;

// [exec.snd], senders
struct sender_t {};

template <class Sndr>
concept sender = std::move_constructible<__remove_cvref_t<Sndr>> && requires {
  typename __remove_cvref_t<Sndr>::sender_concept;
  requires std::same_as<typename __remove_cvref_t<Sndr>::sender_concept, sender_t>;
};

template <class Sndr, class Env = empty_env>
concept sender_in = sender<Sndr> && requires(Sndr&& sndr, Env&& env) {
  { std::forward<Sndr>(sndr).get_completion_signatures(std::forward<Env>(env)) };
};

template <class Sndr, class Rcvr>
concept sender_to = sender<Sndr> && receiver<Rcvr> && requires(Sndr&& sndr, Rcvr&& rcvr) {
  { std::forward<Sndr>(sndr).connect(std::forward<Rcvr>(rcvr)) } -> operation_state;
};

// [exec.connect], sender connection
struct connect_t {
  template <class Sndr, class Rcvr>
    requires sender_to<Sndr, Rcvr>
  constexpr auto operator()(Sndr&& sndr, Rcvr&& rcvr) const
      noexcept(noexcept(std::forward<Sndr>(sndr).connect(std::forward<Rcvr>(rcvr))))
          -> decltype(std::forward<Sndr>(sndr).connect(std::forward<Rcvr>(rcvr))) {
    return std::forward<Sndr>(sndr).connect(std::forward<Rcvr>(rcvr));
  }
};

inline constexpr connect_t connect{};

}  // namespace flow::execution
