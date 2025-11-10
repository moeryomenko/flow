#pragma once

#include <concepts>
#include <type_traits>

namespace flow::execution {

// [exec.opstate], operation states
struct operation_state_t {};

template <class O>
concept operation_state = std::destructible<O> && std::is_object_v<O> && requires {
  typename O::operation_state_concept;
  requires std::same_as<typename O::operation_state_concept, operation_state_t>;
} && requires(O& o) {
  { o.start() } noexcept;
};

struct start_t {
  template <class O>
    requires operation_state<O>
  constexpr void operator()(O& o) const noexcept {
    o.start();
  }
};

inline constexpr start_t start{};

}  // namespace flow::execution
