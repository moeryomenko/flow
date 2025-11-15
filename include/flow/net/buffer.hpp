#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace flow::net {

// [net.buffer] Network buffer types
// Compatible with P2762R2 buffer sequences

// Mutable buffer - represents a modifiable memory region
class mutable_buffer {
 public:
  mutable_buffer() noexcept : data_(nullptr), size_(0) {}

  mutable_buffer(void* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] void* data() const noexcept {
    return data_;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return size_;
  }

  mutable_buffer& operator+=(std::size_t n) noexcept {
    std::size_t offset = std::min(n, size_);
    data_              = static_cast<char*>(data_) + offset;
    size_ -= offset;
    return *this;
  }

 private:
  void*       data_;
  std::size_t size_;
};

// Const buffer - represents a non-modifiable memory region
class const_buffer {
 public:
  const_buffer() noexcept : data_(nullptr), size_(0) {}

  const_buffer(const void* data, std::size_t size) noexcept : data_(data), size_(size) {}

  const_buffer(mutable_buffer const& mb) noexcept : data_(mb.data()), size_(mb.size()) {}

  [[nodiscard]] const void* data() const noexcept {
    return data_;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return size_;
  }

  const_buffer& operator+=(std::size_t n) noexcept {
    std::size_t offset = std::min(n, size_);
    data_              = static_cast<const char*>(data_) + offset;
    size_ -= offset;
    return *this;
  }

 private:
  const void* data_;
  std::size_t size_;
};

// Helper functions for creating buffers
inline mutable_buffer buffer(void* data, std::size_t size) noexcept {
  return {data, size};
}

inline const_buffer buffer(const void* data, std::size_t size) noexcept {
  return {data, size};
}

template <typename T, std::size_t N>
inline mutable_buffer buffer(T (&data)[N]) noexcept {
  return mutable_buffer(data, N * sizeof(T));
}

template <typename T, std::size_t N>
inline const_buffer buffer(const T (&data)[N]) noexcept {
  return const_buffer(data, N * sizeof(T));
}

template <typename T, std::size_t N>
inline mutable_buffer buffer(std::array<T, N>& data) noexcept {
  return mutable_buffer(data.data(), N * sizeof(T));
}

template <typename T, std::size_t N>
inline const_buffer buffer(std::array<T, N> const& data) noexcept {
  return const_buffer(data.data(), N * sizeof(T));
}

template <typename T>
inline mutable_buffer buffer(std::vector<T>& data) noexcept {
  return mutable_buffer(data.data(), data.size() * sizeof(T));
}

template <typename T>
inline const_buffer buffer(std::vector<T> const& data) noexcept {
  return const_buffer(data.data(), data.size() * sizeof(T));
}

template <typename T>
inline mutable_buffer buffer(std::span<T> data) noexcept {
  return mutable_buffer(data.data(), data.size_bytes());
}

template <typename T>
inline const_buffer buffer(std::span<T const> data) noexcept {
  return const_buffer(data.data(), data.size_bytes());
}

// Dynamic buffer sequence for scatter-gather I/O
template <typename Buffer>
class basic_buffer_sequence {
 public:
  using value_type = Buffer;
  using iterator   = typename std::vector<Buffer>::const_iterator;

  basic_buffer_sequence() = default;

  explicit basic_buffer_sequence(Buffer buf) {
    buffers_.push_back(buf);
  }

  basic_buffer_sequence(std::initializer_list<Buffer> bufs) : buffers_(bufs) {}

  void push_back(Buffer buf) {
    buffers_.push_back(buf);
  }

  [[nodiscard]] iterator begin() const noexcept {
    return buffers_.begin();
  }

  [[nodiscard]] iterator end() const noexcept {
    return buffers_.end();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return buffers_.size();
  }

  [[nodiscard]] bool empty() const noexcept {
    return buffers_.empty();
  }

  [[nodiscard]] Buffer const& operator[](std::size_t index) const noexcept {
    return buffers_[index];
  }

  // Calculate total buffer size
  [[nodiscard]] std::size_t total_size() const noexcept {
    std::size_t total = 0;
    for (auto const& buf : buffers_) {
      total += buf.size();
    }
    return total;
  }

 private:
  std::vector<Buffer> buffers_;
};

using mutable_buffer_sequence = basic_buffer_sequence<mutable_buffer>;
using const_buffer_sequence   = basic_buffer_sequence<const_buffer>;

}  // namespace flow::net
