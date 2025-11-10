// customization_point_tests.cpp
// Member function customization point tests (P2855)
// See TESTING_PLAN.md section 4

#include <boost/ut.hpp>
#include <exception>
#include <flow/execution.hpp>

// Define test types at namespace scope
namespace test_types {
struct test_receiver {
  using receiver_concept = flow::execution::receiver_t;

  bool* set_value_called;
  bool* set_error_called;
  bool* set_stopped_called;

  void set_value() && noexcept {
    *set_value_called = true;
  }

  void set_error(std::exception_ptr) && noexcept {
    *set_error_called = true;
  }

  void set_stopped() && noexcept {
    *set_stopped_called = true;
  }
};

struct tagged_receiver {
  using receiver_concept = flow::execution::receiver_t;
  void set_value() && noexcept {}
  void set_error(std::exception_ptr) && noexcept {}
  void set_stopped() && noexcept {}
};

struct tagged_sender {
  using sender_concept = flow::execution::sender_t;

  template <class Env>
  auto get_completion_signatures(Env&&) const {
    return flow::execution::completion_signatures<flow::execution::set_value_t()>{};
  }
};

struct tagged_op_state {
  using operation_state_concept = flow::execution::operation_state_t;
  void start() & noexcept {}
};

struct tagged_scheduler {
  using scheduler_concept = flow::execution::scheduler_t;
  auto schedule() const {
    return tagged_sender{};
  }
  bool operator==(const tagged_scheduler&) const = default;
};
}  // namespace test_types

int main() {
  using namespace boost::ut;
  using namespace flow::execution;
  using namespace test_types;

  "receiver_member_function_calls"_test = [] {
    bool          value_called = false, error_called = false, stopped_called = false;
    test_receiver r{&value_called, &error_called, &stopped_called};

    std::move(r).set_value();
    expect(value_called);
    expect(!error_called);
    expect(!stopped_called);
  };

  "receiver_concept_tag"_test = [] {
    static_assert(requires {
      typename tagged_receiver::receiver_concept;
      requires std::same_as<typename tagged_receiver::receiver_concept,
                            flow::execution::receiver_t>;
    });
  };

  "sender_concept_tag"_test = [] {
    static_assert(requires {
      typename tagged_sender::sender_concept;
      requires std::same_as<typename tagged_sender::sender_concept, flow::execution::sender_t>;
    });
  };

  "operation_state_concept_tag"_test = [] {
    static_assert(requires {
      typename tagged_op_state::operation_state_concept;
      requires std::same_as<typename tagged_op_state::operation_state_concept,
                            flow::execution::operation_state_t>;
    });
  };

  "scheduler_concept_tag"_test = [] {
    static_assert(requires {
      typename tagged_scheduler::scheduler_concept;
      requires std::same_as<typename tagged_scheduler::scheduler_concept,
                            flow::execution::scheduler_t>;
    });
  };
}
