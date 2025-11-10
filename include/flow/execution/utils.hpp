#pragma once

#include <type_traits>

namespace flow::execution {

// [exec.utils], general utilities
template <class T>
using __decay_t = std::decay_t<T>;

template <class T>
using __remove_cvref_t = std::remove_cvref_t<T>;

}  // namespace flow::execution
