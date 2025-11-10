#pragma once

#include <utility>

namespace flow::execution {

// [exec.queries], execution resource queries
enum class forward_progress_guarantee { concurrent, parallel, weakly_parallel };

// Query tag types - forward declarations
struct get_domain_t;
struct get_scheduler_t;
struct get_delegatee_scheduler_t;
struct get_forward_progress_guarantee_t;
template <class CPO>
struct get_completion_scheduler_t;

// Query tag type implementations
struct get_domain_t {
  template <class T>
  constexpr auto operator()(const T& t) const
      noexcept(noexcept(t.query(std::declval<get_domain_t>())))
          -> decltype(t.query(std::declval<get_domain_t>())) {
    return t.query(get_domain_t{});
  }
};

struct get_scheduler_t {
  template <class T>
  constexpr auto operator()(const T& t) const
      noexcept(noexcept(t.query(std::declval<get_scheduler_t>())))
          -> decltype(t.query(std::declval<get_scheduler_t>())) {
    return t.query(get_scheduler_t{});
  }
};

struct get_delegatee_scheduler_t {
  template <class T>
  constexpr auto operator()(const T& t) const
      noexcept(noexcept(t.query(std::declval<get_delegatee_scheduler_t>())))
          -> decltype(t.query(std::declval<get_delegatee_scheduler_t>())) {
    return t.query(get_delegatee_scheduler_t{});
  }
};

struct get_forward_progress_guarantee_t {
  template <class T>
  constexpr auto operator()(const T& t) const
      noexcept(noexcept(t.query(std::declval<get_forward_progress_guarantee_t>())))
          -> decltype(t.query(std::declval<get_forward_progress_guarantee_t>())) {
    return t.query(get_forward_progress_guarantee_t{});
  }

  constexpr auto operator()() const noexcept -> forward_progress_guarantee {
    return forward_progress_guarantee::weakly_parallel;
  }
};

template <class CPO>
struct get_completion_scheduler_t {
  template <class T>
  constexpr auto operator()(const T& t) const
      noexcept(noexcept(t.query(std::declval<get_completion_scheduler_t>())))
          -> decltype(t.query(std::declval<get_completion_scheduler_t>())) {
    return t.query(get_completion_scheduler_t{});
  }
};

inline constexpr get_domain_t                     get_domain{};
inline constexpr get_scheduler_t                  get_scheduler{};
inline constexpr get_delegatee_scheduler_t        get_delegatee_scheduler{};
inline constexpr get_forward_progress_guarantee_t get_forward_progress_guarantee{};

template <class CPO>
inline constexpr get_completion_scheduler_t<CPO> get_completion_scheduler{};

}  // namespace flow::execution
