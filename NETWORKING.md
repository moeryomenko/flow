# Flow Networking Library

## Overview

The Flow networking library provides asynchronous network I/O operations integrated with the C++ `std::execution` sender/receiver model. It is based on three key C++ standardization proposals:

- **P2762R2**: Sender/Receiver Interface For Networking
- **P3185R0**: Transport Services Application Programming Interface (TAPS) for C++
- **P3482R0**: Networking Direction Paper - Adopting IETF TAPS

## Architecture

### Core Design Principles

1. **Sender-Based Asynchronous I/O** (P2762R2)
   - All network operations are sender adaptors
   - Composable with execution algorithms (`then`, `let_value`, `when_all`, etc.)
   - Error reporting via `set_error` channel
   - Cancellation via stop tokens

2. **Property-Based Configuration** (P3482R0)
   - Transport properties describe connection requirements
   - Security properties for TLS/encryption
   - Endpoint properties for addressing
   - Inspired by IETF TAPS (RFC 9621)

3. **Message-Oriented Design** (P3185R0)
   - Focus on complete messages rather than byte streams
   - Buffer sequences for scatter-gather I/O
   - Framer support for protocol handling (future enhancement)

4. **Scheduler Integration**
   - `io_context` provides I/O scheduler
   - Socket operations verify scheduler compatibility
   - Prevents mismatched socket/scheduler usage

## Components

### 1. I/O Context (`io_context`)

Provides the execution context for asynchronous network operations:

```cpp
io_context ctx;
auto scheduler = ctx.get_scheduler();

// Run the event loop
ctx.run();
```

**Key Features:**
- Scheduler for network operations
- Event loop for processing I/O completions
- Unique context ID for socket compatibility verification
- Thread-safe operation posting

### 2. Socket Types

#### TCP Socket (`tcp_socket`)

Stream-oriented connection socket:

```cpp
io_context ctx;
tcp_socket socket(ctx);
socket.open(tcp());

// Async operations
auto work = async_connect(socket, endpoint)
          | then([] { return "Connected!"; });
```

#### TCP Acceptor (`tcp_acceptor`)

Server socket for accepting connections:

```cpp
io_context ctx;
tcp_acceptor acceptor(ctx, tcp());
acceptor.bind(endpoint);
acceptor.listen();

// Accept connections asynchronously (future enhancement)
```

#### UDP Socket (`udp_socket`)

Datagram-oriented socket:

```cpp
io_context ctx;
udp_socket socket(ctx);
socket.open(udp());
socket.bind(endpoint);
```

### 3. Asynchronous Operations

All operations are sender adaptors that integrate with `std::execution`:

#### `async_connect`

Asynchronously connect a stream socket to a peer:

```cpp
tcp_socket socket(ctx);
tcp_endpoint endpoint;  // configured with address:port

auto work = async_connect(socket, endpoint)
          | then([] {
              std::cout << "Connected!\n";
            });
```

**Completion Signatures:**
- `set_value()` - Connection successful
- `set_error(std::error_code)` - Connection failed
- `set_stopped()` - Operation cancelled

#### `async_receive`

Asynchronously receive data from a socket:

```cpp
std::vector<char> buffer(1024);
mutable_buffer_sequence buffers;
buffers.push_back(buffer(buffer.data(), buffer.size()));

auto work = async_receive(socket, buffers)
          | then([](std::size_t bytes_received) {
              std::cout << "Received " << bytes_received << " bytes\n";
            });
```

**Completion Signatures:**
- `set_value(std::size_t)` - Bytes received
- `set_error(std::error_code)` - Receive failed
- `set_stopped()` - Operation cancelled

**With Flags:**
```cpp
auto work = async_receive(socket, message_flags::peek, buffers);
```

#### `async_send`

Asynchronously send data on a socket:

```cpp
std::string message = "Hello, World!";
const_buffer_sequence buffers;
buffers.push_back(buffer(message.data(), message.size()));

auto work = async_send(socket, buffers)
          | then([](std::size_t bytes_sent) {
              std::cout << "Sent " << bytes_sent << " bytes\n";
            });
```

**Completion Signatures:**
- `set_value(std::size_t)` - Bytes sent
- `set_error(std::error_code)` - Send failed
- `set_stopped()` - Operation cancelled

### 4. Buffer Types

#### `mutable_buffer` / `const_buffer`

Represent contiguous memory regions:

```cpp
char data[1024];
auto buf = buffer(data, 1024);  // mutable_buffer

const char* const_data = "Hello";
auto cbuf = buffer(const_data, 5);  // const_buffer
```

#### Buffer Sequences

Collections of buffers for scatter-gather I/O:

```cpp
mutable_buffer_sequence buffers;
buffers.push_back(buffer(buf1, size1));
buffers.push_back(buffer(buf2, size2));
buffers.push_back(buffer(buf3, size3));

// Total size across all buffers
std::size_t total = buffers.total_size();
```

**Supported Container Types:**
```cpp
// Arrays
char arr[1024];
auto buf = buffer(arr);

// std::array
std::array<char, 1024> arr;
auto buf = buffer(arr);

// std::vector
std::vector<char> vec(1024);
auto buf = buffer(vec);

// std::span
std::span<char> span = ...;
auto buf = buffer(span);
```

### 5. Property System (TAPS-Inspired)

#### Transport Properties

Configure transport-layer behavior:

```cpp
// Reliable stream (TCP-like)
auto props = transport_properties::reliable_stream();
props.set_reliability(transport_preference::require);
props.set_preserve_order(transport_preference::require);
props.set_congestion_control(transport_preference::require);

// Unreliable datagram (UDP-like)
auto dgram_props = transport_properties::unreliable_datagram();
dgram_props.set_preserve_msg_boundaries(transport_preference::require);
```

**Transport Preferences:**
- `require` - Must have this property
- `prefer` - Would like to have this property
- `none` - No preference
- `avoid` - Would prefer not to have this property
- `prohibit` - Must not have this property

**Configurable Properties:**
- `reliability` - Reliable delivery guarantee
- `preserve_msg_boundaries` - Message boundary preservation
- `preserve_order` - In-order delivery
- `multistreaming` - Multiple logical streams
- `congestion_control` - Congestion control mechanism
- `keep_alive` - Keep-alive mechanism
- `multipath` - Multipath support (disabled/active/passive)
- `direction` - Bidirectional/send-only/receive-only

#### Security Properties

Configure security and encryption:

```cpp
// TLS 1.3 only
auto security = security_properties::tls_1_3_only();

// HTTP/2 over TLS
auto http2_sec = security_properties::http2_over_tls();
// Sets: TLS 1.2/1.3, ALPN: h2

// Custom configuration
security_properties custom;
custom.set_allowed_protocols({"TLSv1.3"});
custom.set_alpn({"h2", "http/1.1"});
custom.set_max_cached_sessions(100);
```

**Configurable Properties:**
- `allowed_protocols` - TLS/DTLS versions
- `alpn` - Application Layer Protocol Negotiation
- `server_certificate` - Server certificate chain
- `client_certificate` - Client certificate for mutual TLS
- `pinned_server_certificate` - Certificate pinning
- `max_cached_sessions` - Session cache size

#### Endpoint Properties

Configure addressing and endpoints:

```cpp
using namespace endpoint_props;

auto host = hostname("example.com");
auto svc = service::https();  // or service("https")
auto iface = interface_name("eth0");
auto p = port(8080);
```

**Well-Known Services:**
- `service::http()`
- `service::https()`
- `service::ftp()`
- `service::ssh()`

## Usage Patterns

### Pattern 1: Simple Client

```cpp
io_context ctx;
tcp_socket socket(ctx);
tcp_endpoint endpoint;  // Configure with address:port

auto work = async_connect(socket, endpoint)
          | then([] {
              std::cout << "Connected!\n";
              return "Hello, Server!";
            })
          | then([&socket](std::string_view msg) {
              const_buffer_sequence buffers;
              buffers.push_back(buffer(msg.data(), msg.size()));
              return async_send(socket, buffers);
            })
          | then([](std::size_t bytes_sent) {
              std::cout << "Sent " << bytes_sent << " bytes\n";
            });

auto result = this_thread::sync_wait(
    schedule(ctx.get_scheduler()) | then([work = std::move(work)]() mutable {
      return std::move(work);
    })
);
```

### Pattern 2: Request-Response

```cpp
io_context ctx;
tcp_socket socket(ctx);

// Send request, receive response
auto work = schedule(ctx.get_scheduler())
          | then([&] {
              // Send request
              return async_send(socket, request_buffers);
            })
          | let_value([&](std::size_t bytes_sent) {
              // Receive response
              return async_receive(socket, response_buffers);
            })
          | then([](std::size_t bytes_received) {
              // Process response
              return process_response(bytes_received);
            });
```

### Pattern 3: Concurrent Operations

```cpp
io_context ctx;
tcp_socket socket1(ctx);
tcp_socket socket2(ctx);

// Receive from two sockets concurrently
auto recv1 = schedule(ctx.get_scheduler())
           | then([&] { return async_receive(socket1, buffers1); });

auto recv2 = schedule(ctx.get_scheduler())
           | then([&] { return async_receive(socket2, buffers2); });

auto work = when_all(std::move(recv1), std::move(recv2))
          | then([](std::size_t bytes1, std::size_t bytes2) {
              return bytes1 + bytes2;  // Total bytes
            });
```

### Pattern 4: Error Handling

```cpp
auto work = async_receive(socket, buffers)
          | then([](std::size_t bytes) {
              std::cout << "Received " << bytes << " bytes\n";
              return bytes;
            })
          | upon_error([](std::error_code ec) {
              if (ec.value() == ECONNRESET) {
                std::cout << "Connection reset by peer\n";
              } else {
                std::cout << "Error: " << ec.message() << '\n';
              }
              return 0;  // Fallback value
            })
          | upon_error([](std::exception_ptr ep) {
              try {
                std::rethrow_exception(ep);
              } catch (std::exception const& e) {
                std::cout << "Exception: " << e.what() << '\n';
              }
              return 0;
            });
```

### Pattern 5: Pipeable Syntax

```cpp
// Adaptor closure style
auto receive_op = async_receive(socket);
auto work = schedule(ctx.get_scheduler())
          | then([buffers]() { return buffers; })
          | receive_op
          | then([](std::size_t bytes) { return bytes; });
```

## Error Handling

### Error Reporting

All network operations report errors via the `set_error` channel (not fused with `set_value`):

**Error Types:**
1. `std::error_code` - POSIX error codes (ECONNREFUSED, ETIMEDOUT, etc.)
2. `std::exception_ptr` - Exceptional failures

### Common Error Codes

- `ECONNREFUSED` - Connection refused
- `ECONNRESET` - Connection reset by peer
- `ETIMEDOUT` - Connection timed out
- `ENETUNREACH` - Network unreachable
- `EHOSTUNREACH` - Host unreachable
- `EINVAL` - Invalid argument / scheduler mismatch

### Scheduler Compatibility

Operations verify that the socket's context matches the scheduler (P2762R2 §9.1.2.6):

```cpp
io_context ctx1, ctx2;
tcp_socket socket(ctx1);

// ERROR: Scheduler from ctx2 doesn't match socket from ctx1
auto work = schedule(ctx2.get_scheduler())
          | then([&] {
              return async_receive(socket, buffers);  // set_error(EINVAL)
            });
```

## Cancellation

All operations support cancellation via stop tokens:

```cpp
execution::inplace_stop_source stop_source;
auto token = stop_source.get_token();

auto work = async_receive(socket, buffers);

// Start operation with stop token
auto result = this_thread::sync_wait(
    std::move(work),
    execution::make_env_with_stop_token(token, execution::empty_env{})
);

// Request cancellation from another thread
stop_source.request_stop();  // Operation completes with set_stopped()
```

## Composition with Execution Algorithms

Network operations integrate seamlessly with P2300 algorithms:

### `then` - Transform Results

```cpp
async_receive(socket, buffers)
  | then([](std::size_t bytes) {
      return bytes * 2;
    })
```

### `let_value` - Dependent Operations

```cpp
async_send(socket, request)
  | let_value([&](std::size_t) {
      return async_receive(socket, response);
    })
```

### `upon_error` - Error Recovery

```cpp
async_connect(socket, endpoint)
  | upon_error([](std::error_code) {
      return fallback_value;
    })
```

### `when_all` - Concurrent Operations

```cpp
when_all(
  async_receive(socket1, buffers1),
  async_receive(socket2, buffers2)
) | then([](std::size_t b1, std::size_t b2) {
    return b1 + b2;
  })
```

### `retry` - Automatic Retry

```cpp
async_connect(socket, endpoint)
  | retry_n(3)  // Retry up to 3 times on error
```

### `transfer` - Change Scheduler

```cpp
async_receive(socket, buffers)
  | transfer(computation_pool.get_scheduler())
  | then([](std::size_t bytes) {
      // Process on different scheduler
      return process_data(bytes);
    })
```

## Future Enhancements

The current implementation provides a foundation. Future enhancements could include:

### 1. DNS Resolution (P2762R2 §9.4)
```cpp
auto endpoints = async_resolve_name<tcp>("example.com");
```

### 2. TLS/SSL Support (P3482R0)
```cpp
security_properties sec = security_properties::tls_1_3_only();
// Apply to connection
```

### 3. Message Framers (P3185R0, P3482R0)
```cpp
class http_framer {
  // Parse HTTP messages from byte stream
};

auto connection = initiate(preconnection, http_framer{});
```

### 4. Preconnections (P3482R0)
```cpp
preconnection pre;
pre.set_local_endpoint(local_ep);
pre.set_remote_endpoint(remote_ep);
pre.set_transport_properties(props);
pre.set_security_properties(sec);

auto conn = initiate(pre);  // Client connection
auto listener = listen(pre);  // Server connection
```

### 5. async_accept
```cpp
auto accept_work = async_accept(acceptor)
                 | then([](tcp_socket client, tcp_endpoint peer) {
                     // Handle client
                   });
```

### 6. Platform-Specific I/O
- io_uring (Linux)
- kqueue (BSD/macOS)
- IOCP (Windows)

## Design Rationale

### Why Sender-Based I/O?

1. **Composability**: Chain operations naturally
2. **Lazy Execution**: Build work graphs before execution
3. **Cancellation**: Integrated via stop tokens
4. **Error Handling**: Separate error channel
5. **Structured Concurrency**: Works with async scopes

### Why TAPS Properties?

1. **Protocol Agnostic**: Describe requirements, not protocols
2. **Future-Proof**: Extensible to new protocols (QUIC, etc.)
3. **Flexible**: Preferences allow fallback strategies
4. **Declarative**: What you need, not how to achieve it

### Why Message-Oriented?

1. **Natural Abstraction**: Applications think in messages
2. **Protocol Flexibility**: Works with both streams and datagrams
3. **Framing Support**: Can add protocol-specific framing
4. **Better Control**: Deadlines, reliability per message

## References

### C++ Proposals

- [P2300R10](https://wg21.link/p2300r10) - std::execution
- [P2762R2](https://wg21.link/p2762r2) - Sender/Receiver Interface For Networking
- [P3185R0](https://wg21.link/p3185r0) - TAPS for C++ Standard Networking
- [P3482R0](https://wg21.link/p3482r0) - Networking Direction Paper: Adopting IETF TAPS
- [P2855R1](https://wg21.link/p2855r1) - Member customization points

### IETF TAPS

- [RFC 9621](https://www.rfc-editor.org/rfc/rfc9621.html) - TAPS Architecture
- [RFC 9622](https://www.rfc-editor.org/rfc/rfc9622.html) - TAPS Interface
- [RFC 9623](https://www.rfc-editor.org/rfc/rfc9623.html) - TAPS Implementation

## Examples

See `examples/networking_example.cpp` for comprehensive usage examples demonstrating:

- TCP client operations
- Send/receive patterns
- Transport and security properties
- Error handling strategies
- Concurrent operations
- Integration with execution algorithms

## Status

**Current Status**: Experimental / Proof of Concept

This implementation provides a foundation demonstrating the architecture and integration points. A production implementation would require:

- Platform-specific I/O completion mechanisms
- Full scatter-gather I/O support
- DNS resolution
- TLS/SSL integration
- Connection pooling
- Advanced error handling
- Performance optimization

**Compatibility**: Requires C++23 and the Flow execution library.
