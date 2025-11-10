// resource_management_tests.cpp
// Resource management and move/RAII/lifetime tests
// See TESTING_PLAN.md section 11.3

#include <boost/ut.hpp>
#include <flow/execution.hpp>
#include <memory>

int main() {
  using namespace boost::ut;
  using namespace flow::execution;

  "raii_compliance"_test = [] {
    struct resource_tracker {
      int* counter;

      resource_tracker(int* c) : counter(c) {
        (*counter)++;
      }
      ~resource_tracker() {
        if (counter)
          (*counter)--;
      }
      resource_tracker(const resource_tracker&) = delete;
      resource_tracker(resource_tracker&& other) : counter(other.counter) {
        other.counter = nullptr;
      }

      int value() const {
        return 42;
      }
    };

    int resource_count = 0;

    {
      auto s = just(resource_tracker{&resource_count})
               | then([](resource_tracker rt) { return rt.value(); });

      expect(resource_count == 1_i);

      auto result = flow::this_thread::sync_wait(std::move(s));
      expect(std::get<0>(*result) == 42_i);
    }

    expect(resource_count == 0_i);
  };

  "move_semantics"_test = [] {
    struct move_only {
      int   value;
      bool* moved_from;

      move_only(int v, bool* m) : value(v), moved_from(m) {}
      ~move_only() = default;

      move_only(const move_only&)            = delete;
      move_only& operator=(const move_only&) = delete;

      move_only(move_only&& other) : value(other.value), moved_from(other.moved_from) {
        if (other.moved_from)
          *other.moved_from = true;
      }

      move_only& operator=(move_only&&) = default;
    };

    bool was_moved = false;
    auto s         = just(move_only{42, &was_moved}) | then([](move_only mo) { return mo.value; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "unique_ptr_propagation"_test = [] {
    auto s = just(std::make_unique<int>(42)) | then([](std::unique_ptr<int> ptr) { return *ptr; });

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(result.has_value());
    expect(std::get<0>(*result) == 42_i);
  };

  "operation_state_lifetime"_test = [] {
    int constructed = 0;
    int destructed  = 0;

    struct lifetime_tracker {
      int* cons;
      int* dest;

      lifetime_tracker(int* c, int* d) : cons(c), dest(d) {
        (*cons)++;
      }
      ~lifetime_tracker() {
        if (dest)
          (*dest)++;
      }
      lifetime_tracker(const lifetime_tracker&) = delete;
      lifetime_tracker(lifetime_tracker&& other) : cons(other.cons), dest(other.dest) {
        other.dest = nullptr;
      }

      int value() const {
        return 42;
      }
    };

    auto s = just(lifetime_tracker{&constructed, &destructed})
             | then([](lifetime_tracker lt) { return lt.value(); });
    expect(constructed == 1_i);
    expect(destructed == 0_i);

    auto result = flow::this_thread::sync_wait(std::move(s));
    expect(std::get<0>(*result) == 42_i);
    expect(destructed == 1_i);
  };
}
