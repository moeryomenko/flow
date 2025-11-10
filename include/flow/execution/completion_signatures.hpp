#pragma once

#include <utility>

#include "env.hpp"

namespace flow::execution {

// [exec.completion.signatures], completion signatures
template <class... Sigs>
struct completion_signatures {
  using _t = completion_signatures;
};

struct get_completion_signatures_t {
  template <class Sndr, class Env = empty_env>
    requires requires(Sndr&& sndr, Env&& env) {
      std::forward<Sndr>(sndr).get_completion_signatures(std::forward<Env>(env));
    }
  constexpr auto operator()(Sndr&& sndr, Env&& env = {}) const
      noexcept(noexcept(std::forward<Sndr>(sndr).get_completion_signatures(std::forward<Env>(env))))
          -> decltype(std::forward<Sndr>(sndr).get_completion_signatures(std::forward<Env>(env))) {
    return std::forward<Sndr>(sndr).get_completion_signatures(std::forward<Env>(env));
  }
};

inline constexpr get_completion_signatures_t get_completion_signatures{};

}  // namespace flow::execution
