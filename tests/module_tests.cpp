// module_tests.cpp
// Module import and boundary tests for flow::execution
// Tests that the library works correctly whether using traditional
// includes or C++ modules

#include <boost/ut.hpp>

// When building with C++ modules, import the flow module
// Otherwise, use traditional includes
#if defined(__cpp_modules) && __cpp_modules >= 201907L
import flow;
#else
#include <flow/execution.hpp>
#endif

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "core_module_availability"_test = [] {
    // Test that core types are available
    static_assert(sender<decltype(just(42))>);
    static_assert(scheduler<inline_scheduler>);
  };

  "cross_component_interaction"_test = [] {
    // Test interaction between different components
    inline_scheduler sch;
    auto             work = schedule(sch) | then([] { return 42; });

    auto result = flow::this_thread::sync_wait(work);
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "type_visibility"_test = [] {
    // Verify all expected types are visible
    static_assert(requires { typename receiver_t; });
    static_assert(requires { typename sender_t; });
    static_assert(requires { typename scheduler_t; });
    static_assert(requires { typename operation_state_t; });
  };

  "algorithm_availability"_test = [] {
    // Verify algorithms are available
    auto s1 = just(1, 2, 3);
    auto s2 = just(42) | then([](int x) { return x * 2; });
    auto s3 = when_all(just(1), just(2));

    expect(sender<decltype(s1)>);
    expect(sender<decltype(s2)>);
    expect(sender<decltype(s3)>);
  };

  "namespace_organization"_test = [] {
    // Verify proper namespace organization
    using namespace flow::execution;

    // All should be accessible
    auto                  s1 = just(42);
    inline_scheduler      sch;
    [[maybe_unused]] auto s2     = schedule(sch);
    auto                  result = flow::this_thread::sync_wait(std::move(s1));

    expect(result.has_value());
  };
}
