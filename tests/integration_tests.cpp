// integration_tests.cpp
// Integration and scenario tests for flow::execution
// See TESTING_PLAN.md section 14

#include <atomic>
#include <boost/ut.hpp>
#include <chrono>
#include <flow/execution.hpp>
#include <string>
#include <thread>
#include <vector>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "simple_pipeline"_test = [] {
    // Simulate a simple data processing pipeline
    auto pipeline = just(std::vector<int>{1, 2, 3, 4, 5}) | then([](std::vector<int> data) {
                      // Transform: multiply by 2
                      for (auto& x : data)
                        x *= 2;
                      return data;
                    })
                    | then([](std::vector<int> data) {
                        // Filter: keep even numbers (all are even now)
                        return data;
                      })
                    | then([](std::vector<int> data) {
                        // Reduce: sum
                        int sum = 0;
                        for (int x : data)
                          sum += x;
                        return sum;
                      });

    auto result = flow::this_thread::sync_wait(std::move(pipeline));
    expect(result.has_value());
    expect(std::get<0>(*result) == 30_i);  // (1+2+3+4+5)*2 = 30
  };

  "error_recovery_pipeline"_test = [] {
    int attempt = 0;

    auto pipeline = just(42) | then([&](int x) {
                      attempt++;
                      if (attempt == 1) {
                        throw std::runtime_error("temporary failure");
                      }
                      return x * 2;
                    })
                    | upon_error([](std::exception_ptr) {
                        return 0;  // Return fallback value
                      });

    auto result = flow::this_thread::sync_wait(std::move(pipeline));
    expect(result.has_value());
    expect(std::get<0>(*result) == 0_i);  // Error was handled
  };

  "high_throughput"_test = [] {
    const int        num_operations = 1000;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
      threads.emplace_back([&] {
        for (int j = 0; j < num_operations / 10; ++j) {
          auto work   = just(j) | then([](int x) { return x * 2; });
          auto result = flow::this_thread::sync_wait(std::move(work));
          if (result.has_value()) {
            completed.fetch_add(1);
          }
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    expect(completed.load() == num_operations);
  };

  "complex_when_all"_test = [] {
    auto s1 = just(1) | then([](int x) { return x * 2; });
    auto s2 = just(2) | then([](int x) { return x * 3; });
    auto s3 = just(3) | then([](int x) { return x * 4; });

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto result = flow::this_thread::sync_wait(std::move(combined));
    expect(result.has_value());
    // when_all now aggregates values with automatic type deduction
    expect(std::get<0>(*result) == 2_i);   // 1 * 2
    expect(std::get<1>(*result) == 6_i);   // 2 * 3
    expect(std::get<2>(*result) == 12_i);  // 3 * 4
  };

  "resource_cleanup"_test = [] {
    int resource_count = 0;

    struct Resource {
      int* counter;
      Resource(int* c) : counter(c) {
        (*counter)++;
      }
      ~Resource() {
        if (counter)
          (*counter)--;
      }
      Resource(const Resource&) = delete;
      Resource(Resource&& r) : counter(r.counter) {
        r.counter = nullptr;
      }

      int value() const {
        return 42;
      }
    };

    {
      auto s = just(Resource{&resource_count}) | then([](Resource r) { return r.value(); });

      expect(resource_count == 1_i);
      auto result = flow::this_thread::sync_wait(std::move(s));
      expect(result.has_value());
      expect(std::get<0>(*result) == 42_i);
    }

    expect(resource_count == 0_i);
  };

  "when_all_mixed_types"_test = [] {
    // Test when_all with int, string, and double
    auto s1 = just(42);
    auto s2 = just(std::string("hello"));
    auto s3 = just(3.14);

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    // For now, we need explicit types since automatic deduction assumes all int
    auto waiter = flow::this_thread::sync_wait.operator()<int, std::string, double>();
    auto result = waiter(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
    expect(std::get<1>(*result) == std::string("hello"));
    expect(std::get<2>(*result) == 3.14_d);
  };

  "when_all_string_and_bool"_test = [] {
    // Test with bool and string types
    auto s1 = just(true);
    auto s2 = just(std::string("success"));
    auto s3 = just(false);

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto waiter = flow::this_thread::sync_wait.operator()<bool, std::string, bool>();
    auto result = waiter(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == true);
    expect(std::get<1>(*result) == std::string("success"));
    expect(std::get<2>(*result) == false);
  };

  "when_all_with_vectors"_test = [] {
    // Test with vector types
    auto s1 = just(std::vector<int>{1, 2, 3});
    auto s2 = just(std::vector<std::string>{"a", "b"});
    auto s3 = just(std::vector<double>{1.1, 2.2});

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto waiter =
        flow::this_thread::sync_wait
            .operator()<std::vector<int>, std::vector<std::string>, std::vector<double>>();
    auto result = waiter(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result).size() == 3_ul);
    expect(std::get<1>(*result).size() == 2_ul);
    expect(std::get<2>(*result).size() == 2_ul);
    expect(std::get<0>(*result)[0] == 1_i);
    expect(std::get<1>(*result)[0] == std::string("a"));
  };

  "when_all_transformed_different_types"_test = [] {
    // Test when_all where transformations produce different types
    auto s1 = just(10) | then([](int x) { return x * 2; });             // int
    auto s2 = just(5) | then([](int x) { return std::to_string(x); });  // string
    auto s3 = just(7) | then([](int x) { return x / 2.0; });            // double

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto waiter = flow::this_thread::sync_wait.operator()<int, std::string, double>();
    auto result = waiter(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result) == 20_i);
    expect(std::get<1>(*result) == std::string("5"));
    expect(std::get<2>(*result) == 3.5_d);
  };

  "when_all_custom_struct"_test = [] {
    struct Person {
      std::string name;
      int         age;
    };

    struct Product {
      std::string name;
      double      price;
    };

    auto s1 = just(Person{"Alice", 30});
    auto s2 = just(Product{"Book", 19.99});
    auto s3 = just(100);  // int for quantity

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto waiter = flow::this_thread::sync_wait.operator()<Person, Product, int>();
    auto result = waiter(std::move(combined));

    expect(result.has_value());
    expect(std::get<0>(*result).name == std::string("Alice"));
    expect(std::get<0>(*result).age == 30_i);
    expect(std::get<1>(*result).name == std::string("Book"));
    expect(std::get<1>(*result).price == 19.99_d);
    expect(std::get<2>(*result) == 100_i);
  };

  "when_all_then_aggregate_mixed_types"_test = [] {
    // Test when_all followed by then that processes mixed types
    auto s1 = just(42);
    auto s2 = just(std::string("items"));
    auto s3 = just(3.14);

    auto combined =
        when_all(std::move(s1), std::move(s2), std::move(s3))
        | then([](int count, std::string label, double price) {
            return std::to_string(count) + " " + label + " at $" + std::to_string(price);
          });

    auto result = flow::this_thread::sync_wait(std::move(combined));
    expect(result.has_value());
    // Result should be "42 items at $3.140000" (or similar formatting)
    auto& str = std::get<0>(*result);
    expect(str.find("42") != std::string::npos);
    expect(str.find("items") != std::string::npos);
  };

  "when_all_unpacked_values"_test = [] {
    // Demonstrate that when_all sends values as separate arguments (not tuple)
    // and they can be unpacked with structured bindings
    auto s1 = just(10);
    auto s2 = just(20);
    auto s3 = just(30);

    auto combined = when_all(std::move(s1), std::move(s2), std::move(s3));

    auto result = flow::this_thread::sync_wait(std::move(combined));
    expect(result.has_value());

    // Unpack using structured bindings - this proves values are in a tuple
    auto [a, b, c] = *result;
    expect(a == 10_i);
    expect(b == 20_i);
    expect(c == 30_i);
  };

  "when_all_then_receives_unpacked"_test = [] {
    // Demonstrate that then receives unpacked values directly from when_all
    auto combined =
        when_all(just(1), just(2), just(3), just(4)) | then([](int a, int b, int c, int d) {
          // Lambda receives 4 separate int arguments, not a tuple
          return a + b + c + d;
        });

    auto result = flow::this_thread::sync_wait(std::move(combined));
    expect(result.has_value());
    expect(std::get<0>(*result) == 10_i);
  };
}
