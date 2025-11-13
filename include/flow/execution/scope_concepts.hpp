#pragma once

#include <concepts>

#include "completion_signatures.hpp"
#include "receiver.hpp"
#include "sender.hpp"

namespace flow::execution {

// Helper void sender for concept checking
struct __void_sender_for_concept {
  using sender_concept = sender_t;

  template <class Env>
  auto get_completion_signatures(Env&&) const -> completion_signatures<set_value_t()>;

  template <class Rcvr>
  auto connect(Rcvr&& rcvr) -> void;
};

// [exec.scope.assoc], async scope association concept
template <class Assoc>
concept async_scope_association = std::semiregular<Assoc> && requires(Assoc assoc) {
  { assoc.is_associated() } noexcept -> std::same_as<bool>;
  { assoc.disassociate() } noexcept -> std::same_as<void>;
};

// [exec.scope.token], scope token concept
template <class Token>
concept scope_token =
    std::copyable<Token> && requires(Token token, __void_sender_for_concept sndr) {
      { token.try_associate() } -> std::same_as<bool>;
      { token.disassociate() } noexcept -> std::same_as<void>;
      { token.wrap(std::move(sndr)) } -> sender;
    };

// Helper void sender for actual use
struct void_sender {
  using sender_concept = sender_t;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    return completion_signatures<set_value_t()>{};
  }

  template <receiver Rcvr>
  auto connect(Rcvr&& rcvr) {
    struct operation {
      Rcvr rcvr_;
      void start() noexcept {
        std::move(rcvr_).set_value();
      }
    };
    return operation{std::forward<Rcvr>(rcvr)};
  }
};

}  // namespace flow::execution
