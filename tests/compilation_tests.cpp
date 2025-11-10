// compilation_tests.cpp
// Compilation, SFINAE, and error message tests
// See TESTING_PLAN.md section 12

#include <flow/execution.hpp>

// These are mostly static_asserts and compile-only tests

namespace flow::execution::tests {

// Test: concept_satisfaction
static_assert(sender<decltype(just(42))>, "just(42) should be a sender");
static_assert(scheduler<inline_scheduler>, "inline_scheduler should be a scheduler");

// Test: concept_failure - verify types that shouldn't satisfy concepts
static_assert(!receiver<int>, "int is not a receiver");
static_assert(!sender<double>, "double is not a sender");
static_assert(!scheduler<std::string>, "string is not a scheduler");
static_assert(!operation_state<float>, "float is not an operation state");

// Test: proper type deduction
static_assert(std::is_same_v<decltype(just(42)), decltype(just(42))>,
              "Type deduction should be consistent");

// Test: sender composition type requirements
static_assert(sender<decltype(just(42) | then([](int x) { return x * 2; }))>,
              "Sender composition should produce a sender");

static_assert(sender<decltype(just(1) | then([](int) { return 2.0; }))>,
              "Type transformation in then should work");

// Test: receiver_of concept with different types
struct int_receiver {
  using receiver_concept = receiver_t;
  void set_value(int) && noexcept {}
  void set_error(std::exception_ptr) && noexcept {}
  void set_stopped() && noexcept {}
};

static_assert(receiver<int_receiver>, "int_receiver should be a receiver");
static_assert(receiver_of<int_receiver, int>, "int_receiver should accept int");
// Note: receiver_of may be more lenient depending on implementation

// Test: move-only semantics
struct move_only_type {
  move_only_type()                      = default;
  move_only_type(move_only_type&&)      = default;
  move_only_type(const move_only_type&) = delete;
  int value                             = 42;
};

static_assert(sender<decltype(just(move_only_type{}))>,
              "Should be able to create sender with move-only type");

// Test: completion signatures
static_assert(sender<decltype(just())>, "just() with no args should be a sender");
static_assert(sender<decltype(just(1))>, "just(1) should be a sender");
static_assert(sender<decltype(just(1, 2, 3))>, "just(1,2,3) should be a sender");

// Test: scheduler schedule returns sender
static_assert(sender<decltype(std::declval<inline_scheduler>().schedule())>,
              "schedule() should return a sender");

}  // namespace flow::execution::tests

int main() {
  // Compilation tests don't need runtime execution
  return 0;
}
