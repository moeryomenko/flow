#pragma once

namespace flow::execution {

// [exec.env], execution environments
struct empty_env {
  template <class Query>
  friend auto query(const empty_env& /*unused*/, Query /*unused*/) noexcept
      -> decltype(Query::default_value())
    requires requires { Query::default_value(); }
  {
    return Query::default_value();
  }
};

struct get_env_t {
  template <class T>
    requires requires(const T& t) { t.get_env(); }
  constexpr auto operator()(const T& t) const noexcept(noexcept(t.get_env()))
      -> decltype(t.get_env()) {
    return t.get_env();
  }

  template <class T>
  constexpr auto operator()(const T& /*unused*/) const noexcept -> empty_env
    requires(!requires(const T& t) { t.get_env(); })
  {
    return {};
  }
};

inline constexpr get_env_t get_env{};

}  // namespace flow::execution
