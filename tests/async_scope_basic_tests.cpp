#include <boost/ut.hpp>
#include <flow/execution.hpp>

namespace ex = flow::execution;

using namespace boost::ut;

suite scope_token_concept_tests = [] {
  "scope_token concept requirements"_test = [] {
    expect(ex::scope_token<ex::simple_counting_scope::token>);
    expect(ex::scope_token<ex::counting_scope::token>);
  };

  "simple_counting_scope token operations"_test = [] {
    ex::simple_counting_scope* scope = new ex::simple_counting_scope();
    auto                       token = scope->get_token();

    expect(token.try_associate());
    token.disassociate();

    scope->close();
    delete scope;  // Can delete after close with no associations
  };

  "counting_scope token operations"_test = [] {
    ex::counting_scope* scope = new ex::counting_scope();
    auto                token = scope->get_token();

    expect(token.try_associate());
    token.disassociate();

    scope->close();
    delete scope;  // Can delete after close with no associations
  };
};

suite scope_lifecycle_tests = [] {
  "simple_counting_scope lifecycle"_test = [] {
    ex::simple_counting_scope* scope = new ex::simple_counting_scope();
    auto                       token = scope->get_token();

    // Can associate in open state
    expect(token.try_associate());

    // Disassociate before moving forward
    token.disassociate();

    // Can close
    scope->close();

    // Cannot associate after close
    auto token2 = scope->get_token();
    expect(!token2.try_associate());

    delete scope;
  };

  "counting_scope with stop token"_test = [] {
    ex::counting_scope* scope = new ex::counting_scope();

    auto stop_token = scope->get_stop_token();
    expect(!stop_token.stop_requested());

    scope->request_stop();
    expect(stop_token.stop_requested());

    scope->close();
    delete scope;
  };
};

suite async_scope_association_concept_tests = [] {
  "async_scope_association concept check"_test = [] {
    struct test_assoc {
      bool associated_ = false;
      bool is_associated() const noexcept {
        return associated_;
      }
      void disassociate() noexcept {
        associated_ = false;
      }
    };

    expect(ex::async_scope_association<test_assoc>);
  };
};

int main() {
  return 0;
}
