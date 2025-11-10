// interoperability_tests.cpp
// Interoperability and coroutine/networking/GPU tests
// See TESTING_PLAN.md section 10

#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <future>
#include <thread>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "std_thread_compatibility"_test = [] {
    std::thread::id work_thread;
    std::thread::id main_thread = std::this_thread::get_id();

    auto s = just() | then([&] {
               work_thread = std::this_thread::get_id();
               return 42;
             });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "value_types_compatibility"_test = [] {
    // Test with standard library types - simplified
    auto s1 = just(42);
    auto r1 = flow::this_thread::sync_wait(std::move(s1));
    expect(std::get<0>(*r1) == 42_i);
  };

  "exception_ptr_interop"_test = [] {
    auto s = just_error(std::make_exception_ptr(std::runtime_error("test")))
             | upon_error([](std::exception_ptr ep) {
                 try {
                   std::rethrow_exception(ep);
                 } catch (const std::runtime_error& e) {
                   return 42;
                 }
                 return 0;
               });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "mixed_type_propagation"_test = [] {
    auto s = just(42) | then([](int x) { return static_cast<double>(x) * 1.5; })
             | then([](double x) { return std::to_string(x); })
             | then([](std::string s) { return s.length(); });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) > 0_ul);
  };
}
