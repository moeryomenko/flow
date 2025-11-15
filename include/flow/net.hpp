#pragma once

// [net] Networking library based on P2762R2, P3185R0, and P3482R0
//
// This networking library integrates with std::execution (P2300) to provide
// asynchronous network operations using the sender/receiver model.
//
// Key features:
// - Sender-based asynchronous I/O operations (P2762R2)
// - Property-based connection configuration (P3482R0/TAPS)
// - Message-oriented design (P3185R0/TAPS)
// - Integration with execution schedulers
// - Error reporting via set_error channel
// - Cancellation via stop tokens
//
// Main components:
// - io_context: I/O execution context and event loop
// - Socket types: tcp_socket, tcp_acceptor, udp_socket
// - Async operations: async_connect, async_receive, async_send
// - Property system: transport_properties, security_properties
// - Buffer types: mutable_buffer, const_buffer, buffer sequences

#include "net/async_ops.hpp"
#include "net/buffer.hpp"
#include "net/concepts.hpp"
#include "net/io_context.hpp"
#include "net/properties.hpp"
#include "net/socket.hpp"
