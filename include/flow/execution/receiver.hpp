#pragma once

#include <concepts>
#include <utility>

#include "utils.hpp"

namespace flow::execution {

// [exec.recv], receivers
struct receiver_t {};
struct set_value_t {};
struct set_error_t {};
struct set_stopped_t {};

template <class Rcvr>
concept receiver =
    std::move_constructible<__remove_cvref_t<Rcvr>>
    && std::constructible_from<__remove_cvref_t<Rcvr>, Rcvr> && requires {
         typename __remove_cvref_t<Rcvr>::receiver_concept;
         requires std::same_as<typename __remove_cvref_t<Rcvr>::receiver_concept, receiver_t>;
       } && requires(__remove_cvref_t<Rcvr>&& r) {
         { std::move(r).set_stopped() } noexcept;
       };

template <class Rcvr, class... As>
concept receiver_of = receiver<Rcvr> && requires(__remove_cvref_t<Rcvr>&& r, As&&... as) {
  { std::move(r).set_value(std::forward<As>(as)...) } noexcept;
};

// Receiver completion function objects
inline constexpr set_value_t   set_value{};
inline constexpr set_error_t   set_error{};
inline constexpr set_stopped_t set_stopped{};

}  // namespace flow::execution
