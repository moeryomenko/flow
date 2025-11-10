#pragma once

#include <concepts>
#include <utility>

#include "sender.hpp"
#include "utils.hpp"

namespace flow::execution {

// [exec.sched], schedulers
struct scheduler_t {};

template <class Sch>
concept scheduler =
    std::copy_constructible<__remove_cvref_t<Sch>>
    && std::equality_comparable<__remove_cvref_t<Sch>> && requires {
         typename __remove_cvref_t<Sch>::scheduler_concept;
         requires std::same_as<typename __remove_cvref_t<Sch>::scheduler_concept, scheduler_t>;
       } && requires(Sch&& sch) {
         { std::forward<Sch>(sch).schedule() } -> sender;
       };

// [exec.factories], sender factories
struct schedule_t {
  template <scheduler Sch>
  constexpr auto operator()(Sch&& sch) const noexcept(noexcept(std::forward<Sch>(sch).schedule()))
      -> decltype(std::forward<Sch>(sch).schedule()) {
    return std::forward<Sch>(sch).schedule();
  }
};

inline constexpr schedule_t schedule{};

}  // namespace flow::execution
