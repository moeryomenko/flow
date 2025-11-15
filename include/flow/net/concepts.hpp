#pragma once

#include <concepts>
#include <cstdint>
#include <iterator>

#include "../execution/scheduler.hpp"

namespace flow::net {

// Forward declarations
class endpoint;
class socket_base;
template <class Protocol>
class basic_socket;

// [net.concepts] Network concepts

// Concept for types that represent network protocols
template <class Protocol>
concept protocol = requires(Protocol p) {
  typename Protocol::endpoint_type;
  { p.family() } -> std::convertible_to<int>;
  { p.type() } -> std::convertible_to<int>;
  { p.protocol() } -> std::convertible_to<int>;
};

// Concept for types that represent network endpoints
template <class Endpoint>
concept endpoint_type = requires(Endpoint ep) {
  { ep.data() } -> std::convertible_to<const void*>;
  { ep.size() } -> std::convertible_to<std::size_t>;
  { ep.capacity() } -> std::convertible_to<std::size_t>;
  requires requires(Endpoint& mep, std::size_t n) { mep.resize(n); };
};

// Concept for buffer sequences (P2762R2 compatibility)
template <class BufferSequence>
concept MutableBufferSequence = requires(BufferSequence const& buffers) {
  typename BufferSequence::value_type;
  { buffers.begin() };
  { buffers.end() };
  requires std::forward_iterator<decltype(buffers.begin())>;
};

template <class BufferSequence>
concept ConstBufferSequence = requires(BufferSequence const& buffers) {
  typename BufferSequence::value_type;
  { buffers.begin() };
  { buffers.end() };
  requires std::forward_iterator<decltype(buffers.begin())>;
};

// Concept for I/O schedulers (extends execution::scheduler)
// Per P2762R2: networking operations need special I/O context
template <class Scheduler>
concept io_scheduler = execution::scheduler<Scheduler> && requires(Scheduler sch) {
  // I/O schedulers must provide a way to get the underlying context
  // This allows verifying socket and scheduler compatibility
  { sch.context_id() } -> std::convertible_to<std::uintptr_t>;
};

// Concept for sockets
template <class Socket>
concept socket = requires(Socket sock) {
  typename Socket::protocol_type;
  typename Socket::endpoint_type;
  requires protocol<typename Socket::protocol_type>;
  requires endpoint_type<typename Socket::endpoint_type>;

  { sock.native_handle() } -> std::convertible_to<int>;
  { sock.is_open() } -> std::convertible_to<bool>;
};

// Concept for acceptor sockets (P2762R2)
template <class Acceptor>
concept acceptor_socket = socket<Acceptor> && requires(Acceptor acc) {
  typename Acceptor::socket_type;
  requires socket<typename Acceptor::socket_type>;
};

// Concept for stream sockets (P2762R2)
template <class Socket>
concept stream_socket = socket<Socket>;

// Concept for datagram sockets (P2762R2)
template <class Socket>
concept datagram_socket = socket<Socket>;

// Message flags (P2762R2)
enum class message_flags : int {
  none         = 0,
  peek         = 0x01,
  out_of_band  = 0x02,
  do_not_route = 0x04,
};

constexpr message_flags operator|(message_flags a, message_flags b) noexcept {
  return static_cast<message_flags>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr message_flags operator&(message_flags a, message_flags b) noexcept {
  return static_cast<message_flags>(static_cast<int>(a) & static_cast<int>(b));
}

// Wait type for async_wait (P2762R2)
enum class wait_type : int {
  wait_read  = 0x01,
  wait_write = 0x02,
  wait_error = 0x04,
};

constexpr wait_type operator|(wait_type a, wait_type b) noexcept {
  return static_cast<wait_type>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr wait_type operator&(wait_type a, wait_type b) noexcept {
  return static_cast<wait_type>(static_cast<int>(a) & static_cast<int>(b));
}

}  // namespace flow::net
