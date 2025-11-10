#pragma once

namespace flow::execution {

// Type list - a compile-time list of types (not a runtime tuple)
// Used to represent value_types without depending on std::tuple
template <class... Ts>
struct type_list {};

}  // namespace flow::execution
