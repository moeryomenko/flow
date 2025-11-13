#include <boost/ut.hpp>
#include <exception>
#include <flow/execution.hpp>

// Define test types at namespace scope
namespace test_types {
struct minimal_receiver {
  using receiver_concept = flow::execution::receiver_t;
  void set_value() && noexcept {}
  void set_error(std::exception_ptr /*unused*/) && noexcept {}
  void set_stopped() && noexcept {}
};

struct int_receiver {
  using receiver_concept = flow::execution::receiver_t;
  void set_value(int /*unused*/) && noexcept {}
  void set_error(std::exception_ptr /*unused*/) && noexcept {}
  void set_stopped() && noexcept {}
};

struct move_only_receiver {
  using receiver_concept = flow::execution::receiver_t;

  move_only_receiver()                                = default;
  move_only_receiver(move_only_receiver&&)            = default;
  move_only_receiver& operator=(move_only_receiver&&) = default;

  move_only_receiver(const move_only_receiver&)            = delete;
  move_only_receiver& operator=(const move_only_receiver&) = delete;

  void set_value() && noexcept {}
  void set_error(std::exception_ptr /*unused*/) && noexcept {}
  void set_stopped() && noexcept {}
};

struct minimal_sender {
  using sender_concept = flow::execution::sender_t;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    return flow::execution::completion_signatures<flow::execution::set_value_t()>{};
  }
};

struct minimal_operation_state {
  using operation_state_concept = flow::execution::operation_state_t;
  void start() & noexcept {}
};

struct minimal_scheduler {
  using scheduler_concept = flow::execution::scheduler_t;

  [[nodiscard]] static auto schedule() {
    return minimal_sender{};
  }
  bool operator==(const minimal_scheduler&) const = default;
};
}  // namespace test_types

int main() {
  using namespace boost::ut;
  using namespace flow::execution;
  using namespace test_types;

  "receiver_basic_concept"_test = [] {
    static_assert(flow::execution::receiver<minimal_receiver>);
    static_assert(flow::execution::receiver<minimal_receiver&>);
    static_assert(flow::execution::receiver<minimal_receiver&&>);
  };

  "receiver_of_concept"_test = [] {
    static_assert(flow::execution::receiver_of<int_receiver, int>);
    // Note: receiver_of may be lenient in your implementation
  };

  "receiver_movability"_test = [] {
    static_assert(flow::execution::receiver<move_only_receiver>);
    static_assert(std::movable<move_only_receiver>);
    static_assert(!std::copyable<move_only_receiver>);
  };

  "sender_basic_concept"_test = [] { static_assert(flow::execution::sender<minimal_sender>); };

  "operation_state_basic_concept"_test = [] {
    static_assert(flow::execution::operation_state<minimal_operation_state>);
  };

  "scheduler_basic_concept"_test = [] {
    static_assert(flow::execution::scheduler<minimal_scheduler>);
    static_assert(std::copy_constructible<minimal_scheduler>);
    static_assert(std::equality_comparable<minimal_scheduler>);
  };
}
