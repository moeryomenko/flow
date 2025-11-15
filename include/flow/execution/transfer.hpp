#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include "completion_signatures.hpp"
#include "scheduler.hpp"
#include "sender.hpp"
#include "type_list.hpp"
#include "utils.hpp"

namespace flow::execution {

// [exec.transfer], transfer adaptor
// Transitions the execution context of a sender to a different scheduler
namespace _transfer_detail {

// Helper to extract value types from sender
template <class S>
struct _sender_value_types {
  using type = typename S::value_types;
};

template <class S>
using sender_value_types_t = typename _sender_value_types<S>::type;

// Convert type_list to set_value_t signature
template <class TypeList>
struct _type_list_to_set_value;

template <class... Ts>
struct _type_list_to_set_value<type_list<Ts...>> {
  using type = set_value_t(Ts...);
};

template <class TypeList>
using type_list_to_set_value_t = typename _type_list_to_set_value<TypeList>::type;

// Helper to store values from sender
template <class... Ts>
struct _value_storage {
  std::tuple<Ts...> values;

  template <class... Args>
  void store(Args&&... args) {
    values = std::tuple<Ts...>{std::forward<Args>(args)...};
  }

  template <class F>
  auto apply(F&& f) && {
    return std::apply(std::forward<F>(f), std::move(values));
  }
};

// Specialization for empty value list (void)
template <>
struct _value_storage<> {
  void store() {}

  template <class F>
  auto apply(F&& f) && {
    return std::forward<F>(f)();
  }
};

}  // namespace _transfer_detail

// Transfer sender implementation
template <sender S, scheduler Sch>
struct _transfer_sender {
  using sender_concept = sender_t;
  using value_types    = _transfer_detail::sender_value_types_t<S>;

  S   sender_;
  Sch scheduler_;

  template <class Env>
  auto get_completion_signatures(Env&& /*unused*/) const {
    // Transfer preserves value types but may add exception_ptr error
    using set_value_sig = _transfer_detail::type_list_to_set_value_t<value_types>;
    return completion_signatures<set_value_sig, set_error_t(std::exception_ptr), set_stopped_t()>{};
  }

  template <receiver R>
  auto connect(R&& r) && {
    return _transfer_operation<R, S, Sch>{std::move(sender_), std::move(scheduler_),
                                          std::forward<R>(r)};
  }

  template <receiver R>
  auto connect(R&& r) & {
    return _transfer_operation<R, S&, Sch&>{sender_, scheduler_, std::forward<R>(r)};
  }

 private:
  template <class Rcvr, class Sndr, class Sched>
  struct _transfer_operation {
    using operation_state_concept = operation_state_t;

    // Extract value types to store (remove reference from Sndr first)
    using value_types_list = typename __remove_cvref_t<Sndr>::value_types;

    // Create storage based on value types
    template <class TypeList>
    struct _make_storage;

    template <class... Ts>
    struct _make_storage<type_list<Ts...>> {
      using type = _transfer_detail::_value_storage<Ts...>;
    };

    using storage_type = typename _make_storage<value_types_list>::type;

    // Shared state with cleanup callback support
    struct _shared_state {
      Sched                 scheduler_;
      Rcvr                  receiver_;
      storage_type          storage_{};
      std::function<void()> cleanup_;  // Cleanup callback

      template <class Sch2, class R2>
      _shared_state(Sch2&& sch, R2&& r)
          : scheduler_(std::forward<Sch2>(sch)), receiver_(std::forward<R2>(r)) {}

      ~_shared_state() {
        if (cleanup_) {
          cleanup_();
        }
      }
    };

    Sndr                           sender_;
    std::shared_ptr<_shared_state> state_;

    template <class Sndr2, class Sched2, class Rcvr2>
    _transfer_operation(Sndr2&& sndr, Sched2&& sch, Rcvr2&& r)
        : sender_(std::forward<Sndr2>(sndr)),
          state_(
              std::make_shared<_shared_state>(std::forward<Sched2>(sch), std::forward<Rcvr2>(r))) {}

    void start() & noexcept {
      try {
        // Create wrapper for operation state
        using input_op_t =
            decltype(std::forward<Sndr>(sender_).connect(std::declval<_input_receiver>()));

        // Heap-allocate operation state with aligned storage
        struct op_holder {
          alignas(input_op_t) unsigned char storage[sizeof(input_op_t)];
          bool constructed = false;

          ~op_holder() {
            if (constructed) {
              reinterpret_cast<input_op_t*>(storage)->~input_op_t();
            }
          }

          input_op_t* get() {
            return reinterpret_cast<input_op_t*>(storage);
          }
        };

        auto holder_ptr = std::make_shared<op_holder>();

        // Set cleanup callback to delete the operation holder
        state_->cleanup_ = [holder_ptr]() mutable { /* holder_ptr will be destroyed */ };

        // Construct operation in-place
        new (holder_ptr->storage)
            input_op_t(std::forward<Sndr>(sender_).connect(_input_receiver{state_}));
        holder_ptr->constructed = true;

        // Start the operation
        holder_ptr->get()->start();
      } catch (...) {
        std::move(state_->receiver_).set_error(std::current_exception());
      }
    }

   private:
    struct _input_receiver {
      using receiver_concept = receiver_t;

      std::shared_ptr<_shared_state> state_;

      // On successful completion, store values and schedule continuation
      template <class... Args>
      void set_value(Args&&... args) && noexcept {
        try {
          // Store the values
          state_->storage_.store(std::forward<Args>(args)...);

          // Schedule continuation on the new scheduler
          auto schedule_sender = state_->scheduler_.schedule();
          using schedule_op_t =
              decltype(schedule_sender.connect(std::declval<_continuation_receiver>()));

          // Create holder for schedule operation
          struct sched_op_holder {
            alignas(schedule_op_t) unsigned char storage[sizeof(schedule_op_t)];
            bool constructed = false;

            ~sched_op_holder() {
              if (constructed) {
                reinterpret_cast<schedule_op_t*>(storage)->~schedule_op_t();
              }
            }

            schedule_op_t* get() {
              return reinterpret_cast<schedule_op_t*>(storage);
            }
          };

          auto holder_ptr = std::make_shared<sched_op_holder>();

          // Construct schedule operation BEFORE clearing old cleanup
          new (holder_ptr->storage)
              schedule_op_t(schedule_sender.connect(_continuation_receiver{state_}));
          holder_ptr->constructed = true;

          // Now replace the cleanup callback (old one will be destroyed after this line)
          state_->cleanup_ = [holder_ptr]() mutable { /* holder destroyed automatically */ };

          // Start the schedule operation
          holder_ptr->get()->start();
        } catch (...) {
          std::move(state_->receiver_).set_error(std::current_exception());
        }
      }

      // Errors and stopped are propagated directly without scheduling
      template <class E>
      void set_error(E&& e) && noexcept {
        state_->cleanup_ = nullptr;  // Clear cleanup
        std::move(state_->receiver_).set_error(std::forward<E>(e));
      }

      void set_stopped() && noexcept {
        state_->cleanup_ = nullptr;  // Clear cleanup
        std::move(state_->receiver_).set_stopped();
      }
    };

    // Continuation receiver that runs on the new scheduler
    struct _continuation_receiver {
      using receiver_concept = receiver_t;

      std::shared_ptr<_shared_state> state_;

      // When scheduled work completes, send the stored values
      void set_value() && noexcept {
        try {
          // Forward the stored values to the final receiver
          std::move(state_->storage_).apply([this](auto&&... args) {
            std::move(state_->receiver_).set_value(std::forward<decltype(args)>(args)...);
          });

          // Clear cleanup AFTER forwarding values
          state_->cleanup_ = nullptr;
        } catch (...) {
          state_->cleanup_ = nullptr;  // Clear on error
          std::move(state_->receiver_).set_error(std::current_exception());
        }
      }

      template <class E>
      void set_error(E&& e) && noexcept {
        state_->cleanup_ = nullptr;  // Clear cleanup
        std::move(state_->receiver_).set_error(std::forward<E>(e));
      }

      void set_stopped() && noexcept {
        state_->cleanup_ = nullptr;  // Clear cleanup
        std::move(state_->receiver_).set_stopped();
      }
    };
  };
};

// Forward declaration for pipeable support
template <scheduler Sch>
struct _pipeable_transfer;

// Implementation of transfer function object
struct transfer_t {
  // Direct call with sender and scheduler
  template <sender S, scheduler Sch>
  constexpr auto operator()(S&& s, Sch&& sch) const {
    return _transfer_sender<__decay_t<S>, __decay_t<Sch>>{std::forward<S>(s),
                                                          std::forward<Sch>(sch)};
  }

  // Partial application for piping: transfer(scheduler)
  template <scheduler Sch>
  constexpr auto operator()(Sch&& sch) const {
    return _pipeable_transfer<__decay_t<Sch>>{std::forward<Sch>(sch)};
  }
};

// Pipeable adaptor for transfer
template <scheduler Sch>
struct _pipeable_transfer {
  Sch scheduler_;

  explicit _pipeable_transfer(Sch sch) : scheduler_(std::move(sch)) {}

  template <sender S>
  constexpr auto operator()(S&& s) const& {
    return transfer_t{}(std::forward<S>(s), scheduler_);
  }

  template <sender S>
  constexpr auto operator()(S&& s) && {
    return transfer_t{}(std::forward<S>(s), std::move(scheduler_));
  }
};

// Pipe operator support
template <sender S, scheduler Sch>
constexpr auto operator|(S&& s, _pipeable_transfer<Sch>&& p) {
  return std::move(p)(std::forward<S>(s));
}

template <sender S, scheduler Sch>
constexpr auto operator|(S&& s, const _pipeable_transfer<Sch>& p) {
  return p(std::forward<S>(s));
}

inline constexpr transfer_t transfer{};

}  // namespace flow::execution
