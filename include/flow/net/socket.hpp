#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <system_error>

#include "concepts.hpp"
#include "io_context.hpp"

namespace flow::net {

// [net.socket] Basic socket types
// Based on Networking TS with P2762R2 sender/receiver integration

// Socket error category
class socket_category : public std::error_category {
 public:
  [[nodiscard]] const char* name() const noexcept override {
    return "socket";
  }

  [[nodiscard]] std::string message(int ev) const override {
    return std::strerror(ev);
  }

  static socket_category const& instance() {
    static socket_category cat;
    return cat;
  }
};

inline std::error_code make_error_code(int e) {
  return std::error_code(e, socket_category::instance());
}

// IP protocol base
class ip_protocol {
 public:
  constexpr ip_protocol(int family, int type, int protocol) noexcept
      : family_(family), type_(type), protocol_(protocol) {}

  [[nodiscard]] constexpr int family() const noexcept {
    return family_;
  }
  [[nodiscard]] constexpr int type() const noexcept {
    return type_;
  }
  [[nodiscard]] constexpr int protocol() const noexcept {
    return protocol_;
  }

 private:
  int family_;
  int type_;
  int protocol_;
};

// TCP protocol
class tcp : public ip_protocol {
 public:
  constexpr tcp() noexcept : ip_protocol(AF_INET, SOCK_STREAM, IPPROTO_TCP) {}

  class endpoint;
  class socket;
  class acceptor;
};

// UDP protocol
class udp : public ip_protocol {
 public:
  constexpr udp() noexcept : ip_protocol(AF_INET, SOCK_DGRAM, IPPROTO_UDP) {}

  class endpoint;
  class socket;
};

// Basic endpoint (simplified)
template <class Protocol>
class basic_endpoint {
 public:
  using protocol_type = Protocol;

  basic_endpoint() noexcept = default;

  [[nodiscard]] void* data() noexcept {
    return &storage_;
  }
  [[nodiscard]] const void* data() const noexcept {
    return &storage_;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return size_;
  }
  [[nodiscard]] std::size_t capacity() const noexcept {
    return sizeof(storage_);
  }

  void resize(std::size_t new_size) noexcept {
    size_ = std::min(new_size, sizeof(storage_));
  }

 private:
  sockaddr_storage storage_{};
  std::size_t      size_{};
};

// Basic socket implementation
template <class Protocol>
class basic_socket {
 public:
  using protocol_type      = Protocol;
  using endpoint_type      = basic_endpoint<Protocol>;
  using native_handle_type = int;

  explicit basic_socket(io_context& ctx) : ctx_(&ctx), handle_(-1) {}

  basic_socket(io_context& ctx, protocol_type const& protocol) : ctx_(&ctx), handle_(-1) {
    open(protocol);
  }

  basic_socket(io_context& ctx, protocol_type const& protocol, native_handle_type handle)
      : ctx_(&ctx), handle_(handle) {}

  ~basic_socket() {
    close();
  }

  basic_socket(basic_socket const&)            = delete;
  basic_socket& operator=(basic_socket const&) = delete;

  basic_socket(basic_socket&& other) noexcept
      : ctx_(other.ctx_), handle_(other.handle_), protocol_(other.protocol_) {
    other.handle_ = -1;
  }

  basic_socket& operator=(basic_socket&& other) noexcept {
    if (this != &other) {
      close();
      ctx_          = other.ctx_;
      handle_       = other.handle_;
      protocol_     = other.protocol_;
      other.handle_ = -1;
    }
    return *this;
  }

  // Get scheduler from context (for P2762R2 §9.1.2.6)
  [[nodiscard]] io_context::scheduler_type get_scheduler() const noexcept {
    return ctx_->get_scheduler();
  }

  [[nodiscard]] std::uintptr_t context_id() const noexcept {
    return ctx_->context_id();
  }

  [[nodiscard]] native_handle_type native_handle() const noexcept {
    return handle_;
  }

  [[nodiscard]] bool is_open() const noexcept {
    return handle_ != -1;
  }

  void open(protocol_type const& protocol) {
    if (is_open()) {
      close();
    }
    handle_ = ::socket(protocol.family(), protocol.type(), protocol.protocol());
    if (handle_ < 0) {
      throw std::system_error(make_error_code(errno));
    }
    protocol_ = protocol;
  }

  void close() noexcept {
    if (handle_ != -1) {
      ::close(handle_);
      handle_ = -1;
    }
  }

  void bind(endpoint_type const& endpoint) {
    if (::bind(handle_, static_cast<const sockaddr*>(endpoint.data()), endpoint.size()) < 0) {
      throw std::system_error(make_error_code(errno));
    }
  }

 protected:
  io_context*        ctx_;
  native_handle_type handle_;
  protocol_type      protocol_;
};

// Stream socket (for TCP)
template <class Protocol>
class basic_stream_socket : public basic_socket<Protocol> {
 public:
  using basic_socket<Protocol>::basic_socket;

  void connect(typename basic_socket<Protocol>::endpoint_type const& endpoint) {
    if (::connect(this->handle_, static_cast<const sockaddr*>(endpoint.data()), endpoint.size())
        < 0) {
      throw std::system_error(make_error_code(errno));
    }
  }

  void shutdown(int how = SHUT_RDWR) {
    if (::shutdown(this->handle_, how) < 0) {
      throw std::system_error(make_error_code(errno));
    }
  }
};

// Acceptor socket (for TCP servers)
template <class Protocol>
class basic_socket_acceptor : public basic_socket<Protocol> {
 public:
  using socket_type = basic_stream_socket<Protocol>;

  using basic_socket<Protocol>::basic_socket;

  void listen(int backlog = SOMAXCONN) {
    if (::listen(this->handle_, backlog) < 0) {
      throw std::system_error(make_error_code(errno));
    }
  }

  socket_type accept() {
    typename socket_type::endpoint_type peer_endpoint;
    socklen_t                           peer_len = peer_endpoint.capacity();

    int client_fd =
        ::accept(this->handle_, static_cast<sockaddr*>(peer_endpoint.data()), &peer_len);

    if (client_fd < 0) {
      throw std::system_error(make_error_code(errno));
    }

    peer_endpoint.resize(peer_len);
    return socket_type(*this->ctx_, this->protocol_, client_fd);
  }
};

// Datagram socket (for UDP)
template <class Protocol>
class basic_datagram_socket : public basic_socket<Protocol> {
 public:
  using basic_socket<Protocol>::basic_socket;
};

// Concrete socket types
using tcp_socket   = basic_stream_socket<tcp>;
using tcp_acceptor = basic_socket_acceptor<tcp>;
using tcp_endpoint = basic_endpoint<tcp>;
using udp_socket   = basic_datagram_socket<udp>;
using udp_endpoint = basic_endpoint<udp>;

}  // namespace flow::net
