// Tests for let_async_scope implementation (P3296R4)

#include <boost/ut.hpp>
#include <flow/execution.hpp>

using namespace boost::ut;

int main() {
  "let_async_scope_basic"_test = [] {
    using namespace flow::execution;

    bool executed = false;

    // Basic usage: scope automatically joins
    flow::this_thread::sync_wait(just() | let_async_scope([&](auto scope_token) {
                                   spawn(just() | then([&] { executed = true; }), scope_token);
                                   return just();
                                 }));
    expect(executed);
  };

  "let_async_scope_multiple_spawns"_test = [] {
    using namespace flow::execution;

    std::atomic<int> counter{0};

    // Spawn multiple tasks
    flow::this_thread::sync_wait(just() | let_async_scope([&](auto scope_token) {
                                   for (int i = 0; i < 5; ++i) {
                                     spawn(just() | then([&] { counter++; }), scope_token);
                                   }
                                   return just();
                                 }));
    expect(counter == 5);
  };

  return 0;
}
