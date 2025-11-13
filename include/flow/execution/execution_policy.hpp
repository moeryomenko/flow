#pragma once

#include <type_traits>

namespace flow::execution {

// Forward declarations
struct sequenced_policy;
struct parallel_policy;
struct parallel_unsequenced_policy;
struct unsequenced_policy;

// Execution policy tags
struct sequenced_policy {
  static constexpr bool is_seq = true;
  static constexpr bool is_par = false;
  static constexpr bool is_vec = false;
};

struct parallel_policy {
  static constexpr bool is_seq = false;
  static constexpr bool is_par = true;
  static constexpr bool is_vec = false;
};

struct parallel_unsequenced_policy {
  static constexpr bool is_seq = false;
  static constexpr bool is_par = true;
  static constexpr bool is_vec = true;
};

struct unsequenced_policy {
  static constexpr bool is_seq = false;
  static constexpr bool is_par = false;
  static constexpr bool is_vec = true;
};

// Global execution policy objects
inline constexpr sequenced_policy            seq{};
inline constexpr parallel_policy             par{};
inline constexpr parallel_unsequenced_policy par_unseq{};
inline constexpr unsequenced_policy          unseq{};

// Trait to check if a type is an execution policy
template <class T>
struct is_execution_policy : std::false_type {};

template <>
struct is_execution_policy<sequenced_policy> : std::true_type {};

template <>
struct is_execution_policy<parallel_policy> : std::true_type {};

template <>
struct is_execution_policy<parallel_unsequenced_policy> : std::true_type {};

template <>
struct is_execution_policy<unsequenced_policy> : std::true_type {};

template <class T>
inline constexpr bool is_execution_policy_v = is_execution_policy<std::remove_cvref_t<T>>::value;

}  // namespace flow::execution
