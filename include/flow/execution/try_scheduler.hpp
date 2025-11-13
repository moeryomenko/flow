#pragma once

#include <concepts>
#include <utility>

#include "scheduler.hpp"
#include "sender.hpp"
#include "utils.hpp"

namespace flow::execution {

// [exec.sched.try], try schedulers (P3669R2)

// Tag type for try_scheduler concept
struct try_scheduler_t {};

// Error type for when non-blocking operation would block
struct would_block_t {
  auto operator==(const would_block_t&) const noexcept -> bool = default;
};

// [exec.sched.try.concept], try_scheduler concept
template <class Sch>
concept try_scheduler =
    std::derived_from<typename __remove_cvref_t<Sch>::try_scheduler_concept, try_scheduler_t>
    && scheduler<Sch> && requires(Sch&& sch) {
         { std::forward<Sch>(sch).try_schedule() } -> sender;
       };

// [exec.factories.try_schedule], try_schedule sender factory
struct try_schedule_t {
  template <try_scheduler Sch>
  constexpr auto operator()(Sch&& sch) const
      noexcept(noexcept(std::forward<Sch>(sch).try_schedule()))
          -> decltype(std::forward<Sch>(sch).try_schedule()) {
    return std::forward<Sch>(sch).try_schedule();
  }
};

inline constexpr try_schedule_t try_schedule{};

}  // namespace flow::execution
