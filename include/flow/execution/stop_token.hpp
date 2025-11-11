#pragma once

#include <atomic>
#include <stop_token>
#include <type_traits>
#include <utility>

#include "flow/execution/utils.hpp"

namespace flow::execution {

// Forward declarations
class inplace_stop_source;
class inplace_stop_token;
template <class Callback>
class inplace_stop_callback;

// Query tag for stop tokens
struct get_stop_token_t {
  template <class Env>
  auto operator()(const Env& env) const noexcept -> decltype(auto) {
    if constexpr (requires { query(env, get_stop_token_t{}); }) {
      return query(env, get_stop_token_t{});
    } else {
      return std::stop_token{};
    }
  }
};

inline constexpr get_stop_token_t get_stop_token{};

// Helper to get stop token type from environment
template <class Env>
using stop_token_of_t = decltype(get_stop_token(std::declval<Env>()));

// inplace_stop_token implementation
// A lightweight, non-allocating stop token
class inplace_stop_token {
 public:
  inplace_stop_token() noexcept = default;

  explicit inplace_stop_token(const inplace_stop_source* source) noexcept : source_(source) {}

  [[nodiscard]] bool stop_requested() const noexcept;

  [[nodiscard]] bool stop_possible() const noexcept {
    return source_ != nullptr;
  }

  friend bool operator==(const inplace_stop_token& lhs, const inplace_stop_token& rhs) noexcept {
    return lhs.source_ == rhs.source_;
  }

  friend bool operator!=(const inplace_stop_token& lhs, const inplace_stop_token& rhs) noexcept {
    return !(lhs == rhs);
  }

 private:
  friend class inplace_stop_source;
  template <class>
  friend class inplace_stop_callback;

  const inplace_stop_source* source_ = nullptr;
};

// inplace_stop_source implementation
class inplace_stop_source {
 public:
  inplace_stop_source() noexcept = default;

  ~inplace_stop_source() = default;

  inplace_stop_source(const inplace_stop_source&)            = delete;
  inplace_stop_source& operator=(const inplace_stop_source&) = delete;

  inplace_stop_source(inplace_stop_source&&)            = delete;
  inplace_stop_source& operator=(inplace_stop_source&&) = delete;

  [[nodiscard]] inplace_stop_token get_token() const noexcept {
    return inplace_stop_token{this};
  }

  bool request_stop() noexcept {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      // Execute callbacks
      execute_callbacks();
      return true;
    }
    return false;
  }

  [[nodiscard]] bool stop_requested() const noexcept {
    return stopped_.load(std::memory_order_acquire);
  }

 private:
  template <class>
  friend class inplace_stop_callback;

  struct callback_base {
    callback_base* next    = nullptr;
    callback_base* prev    = nullptr;
    bool*          removed = nullptr;

    virtual void execute() noexcept = 0;

   protected:
    ~callback_base() = default;
  };

  void register_callback(callback_base* cb) noexcept {
    if (stopped_.load(std::memory_order_acquire)) {
      // Already stopped - execute immediately
      cb->execute();
      return;
    }

    // Add to linked list
    cb->next = callbacks_;
    if (callbacks_ != nullptr) {
      callbacks_->prev = cb;
    }
    callbacks_ = cb;
  }

  void unregister_callback(callback_base* cb) noexcept {
    if (cb->removed != nullptr) {
      *cb->removed = true;
    }

    if (cb->prev != nullptr) {
      cb->prev->next = cb->next;
    } else {
      callbacks_ = cb->next;
    }

    if (cb->next != nullptr) {
      cb->next->prev = cb->prev;
    }
  }

  void execute_callbacks() noexcept {
    callback_base* cb = callbacks_;
    while (cb != nullptr) {
      callback_base* next = cb->next;
      cb->execute();
      cb = next;
    }
  }

  std::atomic<bool> stopped_{false};
  callback_base*    callbacks_ = nullptr;
};

// inplace_stop_callback implementation
template <class Callback>
class inplace_stop_callback final : private inplace_stop_source::callback_base {
 public:
  template <class CB>
    requires std::constructible_from<Callback, CB>
  explicit inplace_stop_callback(inplace_stop_token token,
                                 CB&& cb) noexcept(std::is_nothrow_constructible_v<Callback, CB>)
      : callback_(std::forward<CB>(cb)), source_(token.source_) {
    if (source_ != nullptr) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      const_cast<inplace_stop_source*>(source_)->register_callback(this);
    }
  }

  ~inplace_stop_callback() {
    if (source_ != nullptr && !removed_) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      const_cast<inplace_stop_source*>(source_)->unregister_callback(this);
    }
  }

  inplace_stop_callback(const inplace_stop_callback&)            = delete;
  inplace_stop_callback& operator=(const inplace_stop_callback&) = delete;

  inplace_stop_callback(inplace_stop_callback&&)            = delete;
  inplace_stop_callback& operator=(inplace_stop_callback&&) = delete;

 private:
  void execute() noexcept override {
    callback_();
  }

  Callback                   callback_;
  const inplace_stop_source* source_;
  bool                       removed_ = false;
};

inline bool inplace_stop_token::stop_requested() const noexcept {
  return source_ != nullptr && source_->stop_requested();
}

// Helper to create stop callbacks
template <class Token, class Callback>
struct _stop_callback_for {
  using type = inplace_stop_callback<Callback>;
};

template <class Callback>
struct _stop_callback_for<std::stop_token, Callback> {
  using type = std::stop_callback<Callback>;
};

template <class Token, class Callback>
using stop_callback_for_t = typename _stop_callback_for<Token, Callback>::type;

// Environment wrapper that adds a stop token
template <class StopToken, class BaseEnv>
struct env_with_stop_token {
  StopToken stop_token;
  BaseEnv   base_env;

  template <class Query>
    requires std::same_as<Query, get_stop_token_t>
  friend auto query(const env_with_stop_token& self, Query /*unused*/) noexcept -> StopToken {
    return self.stop_token;
  }

  template <class Query>
    requires(!std::same_as<Query, get_stop_token_t>)
            && requires(const BaseEnv& env, Query q) { query(env, q); }
  friend auto query(const env_with_stop_token& self,
                    Query                      q) noexcept(noexcept(query(self.base_env, q)))
      -> decltype(query(self.base_env, q)) {
    return query(self.base_env, q);
  }
};

// Helper to create an environment with stop token
template <class StopToken, class BaseEnv>
auto make_env_with_stop_token(StopToken token, BaseEnv&& base) {
  return env_with_stop_token<StopToken, __decay_t<BaseEnv>>{std::move(token),
                                                            std::forward<BaseEnv>(base)};
}

}  // namespace flow::execution
