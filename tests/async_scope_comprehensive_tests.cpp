#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>
#include <thread>
#include <vector>

namespace ex = flow::execution;
namespace tt = flow::this_thread;

using namespace boost::ut;

// ============================================================================
// 1. Core Concept Tests
// ============================================================================

const suite core_concept_tests = [] {
  "scope_token concept requirements"_test = [] {
    expect(ex::scope_token<ex::simple_counting_scope::token>);
    expect(ex::scope_token<ex::counting_scope::token>);

    ex::simple_counting_scope scope;
    auto                      token1 = scope.get_token();
    auto                      token2 = token1;  // Copy constructor
    token2                           = token1;  // Copy assignment

    expect(noexcept(token1.try_associate()));
    expect(noexcept(token1.disassociate()));
  };

  "async_scope_association concept"_test = [] {
    struct test_association {
      bool               associated_ = false;
      [[nodiscard]] bool is_associated() const noexcept {
        return associated_;
      }
      void disassociate() noexcept {
        associated_ = false;
      }
    };

    expect(ex::async_scope_association<test_association>);

    test_association assoc{true};
    expect(assoc.is_associated());
    assoc.disassociate();
    expect(!assoc.is_associated());
  };
};

// ============================================================================
// 2. Simple Counting Scope State Machine Tests
// ============================================================================

const suite simple_counting_scope_state_tests = [] {
  "unused to open transition"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    expect(token.try_associate());
    token.disassociate();
    scope.close();  // Must close before destruction since scope is in open state
  };

  "unused to unused-and-closed transition"_test = [] {
    ex::simple_counting_scope scope;
    scope.close();

    auto token = scope.get_token();
    expect(!token.try_associate());
  };

  "open to unused-and-closed transition"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    expect(token.try_associate());
    token.disassociate();

    scope.close();
    expect(!token.try_associate());
  };

  "multiple associations"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    expect(token.try_associate());
    expect(token.try_associate());
    expect(token.try_associate());

    token.disassociate();
    token.disassociate();
    token.disassociate();

    scope.close();
  };

  "join with zero count"_test = [] {
    ex::simple_counting_scope scope;
    auto                      result = tt::sync_wait(scope.join());
    expect(result.has_value());
  };

  "multiple tokens from same scope"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token1 = scope.get_token();
    auto                      token2 = scope.get_token();

    expect(token1.try_associate());
    expect(token2.try_associate());

    token1.disassociate();
    token2.disassociate();

    scope.close();
  };
};

// ============================================================================
// 3. Counting Scope Tests
// ============================================================================

const suite counting_scope_tests = [] {
  "stop source integration"_test = [] {
    ex::counting_scope scope;

    auto stop_token = scope.get_stop_token();
    expect(!stop_token.stop_requested());

    scope.request_stop();
    expect(stop_token.stop_requested());
  };

  "stop token propagation"_test = [] {
    ex::counting_scope scope;

    std::atomic<bool> stopped{false};
    std::atomic<bool> started{false};

    auto check_thread = std::thread([&, st = scope.get_stop_token()] {
      started.store(true);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (st.stop_requested()) {
        stopped.store(true);
      }
    });

    while (!started.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    scope.request_stop();
    check_thread.join();

    expect(stopped.load());
  };

  "token operations"_test = [] {
    ex::counting_scope scope;
    auto               token = scope.get_token();

    expect(token.try_associate());
    token.disassociate();

    scope.close();
  };

  "join behavior"_test = [] {
    ex::counting_scope scope;
    auto               result = tt::sync_wait(scope.join());
    expect(result.has_value());
  };
};

// ============================================================================
// 4. Thread Safety Tests
// ============================================================================

const suite thread_safety_tests = [] {
  "concurrent associations"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    std::atomic<int>         successful{0};
    std::vector<std::thread> threads;

    threads.reserve(10);
    for (int i = 0; i < 10; ++i) {
      threads.emplace_back([token, &successful]() mutable {
        if (token.try_associate()) {
          successful.fetch_add(1);
          token.disassociate();
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    expect(successful.load() == 10_i);
    scope.close();
    tt::sync_wait(scope.join());
  };

  "close during associations"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    std::atomic<bool> keep_running{true};
    std::atomic<int>  successful{0};
    std::atomic<int>  failed{0};

    std::thread worker([token, &keep_running, &successful, &failed]() mutable {
      while (keep_running.load()) {
        if (token.try_associate()) {
          successful.fetch_add(1);
          token.disassociate();
        } else {
          failed.fetch_add(1);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    scope.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Give time for failed attempts
    keep_running.store(false);

    worker.join();

    expect(successful.load() > 0_i);
    expect(failed.load() > 0_i);

    // Need to join to transition to joined state
    tt::sync_wait(scope.join());
  };
};

// ============================================================================
// 5. Corner Cases and Edge Conditions
// ============================================================================

const suite corner_case_tests = [] {
  "empty scope lifecycle"_test = [] {
    ex::simple_counting_scope scope;
    scope.close();
    auto result = tt::sync_wait(scope.join());
    expect(result.has_value());
  };

  "immediate join"_test = [] {
    ex::counting_scope scope;
    auto               result = tt::sync_wait(scope.join());
    expect(result.has_value());
  };

  "multiple close calls"_test = [] {
    ex::simple_counting_scope scope;
    scope.close();
    scope.close();
    scope.close();
  };

  "token after scope close"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    scope.close();
    expect(!token.try_associate());
  };

  "many associations"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    for (int i = 0; i < 1000; ++i) {
      expect(token.try_associate());
      token.disassociate();
    }

    scope.close();
  };

  "stop before operations"_test = [] {
    ex::counting_scope scope;
    scope.request_stop();

    auto token      = scope.get_token();
    auto stop_token = scope.get_stop_token();

    expect(stop_token.stop_requested());
    expect(token.try_associate());
    token.disassociate();

    scope.close();
  };

  "token copy semantics"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token1 = scope.get_token();
    auto                      token2 = token1;

    expect(token1.try_associate());
    expect(token2.try_associate());

    token1.disassociate();
    token2.disassociate();

    scope.close();
  };

  "close with open state and zero count"_test = [] {
    ex::simple_counting_scope scope;
    auto                      token = scope.get_token();

    expect(token.try_associate());
    token.disassociate();

    scope.close();
    expect(!token.try_associate());
  };

  "join after close"_test = [] {
    ex::simple_counting_scope scope;
    scope.close();

    auto result = tt::sync_wait(scope.join());
    expect(result.has_value());
  };

  "multiple concurrent get_token calls"_test = [] {
    ex::counting_scope       scope;
    std::vector<std::thread> threads;
    std::atomic<int>         token_count{0};

    threads.reserve(20);
    for (int i = 0; i < 20; ++i) {
      threads.emplace_back([&scope, &token_count] {
        auto token = scope.get_token();
        token_count.fetch_add(1);
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    expect(token_count.load() == 20_i);
    scope.close();
  };
};

int main() {
  return 0;
}
