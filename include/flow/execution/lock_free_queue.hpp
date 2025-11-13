#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace flow::execution {

// Lock-free bounded MPMC (Multiple Producer Multiple Consumer) queue
// Uses a fixed-size ring buffer to avoid allocations - truly non-blocking
// Suitable for try_schedule non-blocking operations
template <typename T, std::size_t Capacity = 1024>
class lock_free_bounded_queue {
 public:
  lock_free_bounded_queue() : head_(0), tail_(0) {
    // Initialize all slots as empty
    for (std::size_t i = 0; i < Capacity; ++i) {
      slots_[i].version.store(i, std::memory_order_relaxed);
    }
  }

  ~lock_free_bounded_queue() = default;

  lock_free_bounded_queue(const lock_free_bounded_queue&)                    = delete;
  auto operator=(const lock_free_bounded_queue&) -> lock_free_bounded_queue& = delete;

  // Try to push an item. Returns false if queue is full
  // This is signal-safe and truly non-blocking - no allocations
  auto try_push(T&& value) noexcept -> bool {
    std::size_t tail = tail_.load(std::memory_order_relaxed);

    for (;;) {
      slot&       s       = slots_[tail % Capacity];
      std::size_t version = s.version.load(std::memory_order_acquire);

      // Check if this slot is ready for writing
      std::ptrdiff_t diff =
          static_cast<std::ptrdiff_t>(version) - static_cast<std::ptrdiff_t>(tail);

      if (diff == 0) {
        // Slot is available, try to claim it
        if (tail_.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed)) {
          // We claimed the slot, now write the data
          // Note: T's move constructor might throw (e.g., std::function)
          // This is caught by the caller
          new (s.storage.data()) T(std::move(value));
          s.version.store(tail + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        // Queue is full
        return false;
      } else {
        // Another thread is using this slot, try next
        tail = tail_.load(std::memory_order_relaxed);
      }
    }
  }

  // Try to pop an item. Returns nullopt if queue is empty
  auto try_pop() noexcept -> std::optional<T> {
    std::size_t head = head_.load(std::memory_order_relaxed);

    for (;;) {
      slot&       s       = slots_[head % Capacity];
      std::size_t version = s.version.load(std::memory_order_acquire);

      // Check if this slot has data to read
      std::ptrdiff_t diff =
          static_cast<std::ptrdiff_t>(version) - static_cast<std::ptrdiff_t>(head + 1);

      if (diff == 0) {
        // Slot has data, try to claim it
        if (head_.compare_exchange_weak(head, head + 1, std::memory_order_relaxed)) {
          // We claimed the slot, read the data
          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-init-variables)
          T* const ptr   = reinterpret_cast<T*>(s.storage.data());
          T        value = std::move(*ptr);
          ptr->~T();
          s.version.store(head + Capacity, std::memory_order_release);
          return value;
        }
      } else if (diff < 0) {
        // Queue is empty
        return std::nullopt;
      } else {
        // Another thread is using this slot, try next
        head = head_.load(std::memory_order_relaxed);
      }
    }
  }

  // Check if queue is empty (may not be accurate due to concurrent access)
  auto empty() const noexcept -> bool {
    std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t tail = tail_.load(std::memory_order_acquire);
    return head == tail;
  }

  // Check if queue is full (may not be accurate due to concurrent access)
  auto full() const noexcept -> bool {
    std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t tail = tail_.load(std::memory_order_acquire);
    return (tail - head) >= Capacity;
  }

 private:
  struct slot {
    std::atomic<std::size_t> version;
    alignas(T) std::array<unsigned char, sizeof(T)> storage{};
  };

  alignas(64) std::atomic<std::size_t> head_;  // Consumer side (cache line aligned)
  alignas(64) std::atomic<std::size_t> tail_;  // Producer side (cache line aligned)
  std::array<slot, Capacity> slots_;
};

}  // namespace flow::execution
