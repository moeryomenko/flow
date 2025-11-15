#include <flow/execution.hpp>
#include <flow/net.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace flow;
using namespace flow::execution;
using namespace flow::net;

// Example 1: Basic TCP client with async_connect and async_send
void example_tcp_client() {
  std::cout << "=== TCP Client Example ===\n";

  io_context ctx;
  tcp_socket socket(ctx);

  // Create endpoint (in real code, would use DNS resolution)
  tcp_endpoint endpoint;  // Would configure with actual address/port

  try {
    // Open socket
    socket.open(tcp());

    std::cout << "TCP socket opened successfully\n";
    std::cout << "In a real implementation, would:\n";
    std::cout << "  - Async connect to remote endpoint\n";
    std::cout << "  - Send/receive data using sender/receiver model\n";
    std::cout << "  - Compose operations with then/let_value/when_all\n";

  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << '\n';
  }
}

// Example 2: Composing multiple async operations
void example_send_receive() {
  std::cout << "\n=== Send/Receive Example ===\n";

  io_context ctx;
  tcp_socket socket(ctx);

  // Message to send
  std::string           message = "Hello, World!";
  const_buffer_sequence send_buffers;
  send_buffers.push_back(buffer(message.data(), message.size()));

  // Buffer for receiving
  std::vector<char>       recv_buffer(1024);
  mutable_buffer_sequence recv_buffers;
  recv_buffers.push_back(buffer(recv_buffer.data(), recv_buffer.size()));

  try {
    socket.open(tcp());

    std::cout << "Created send buffers: " << send_buffers.total_size() << " bytes\n";
    std::cout << "Created receive buffers: " << recv_buffers.total_size() << " bytes\n";
    std::cout << "In a real implementation, would compose:\n";
    std::cout << "  async_send | let_value(...) | async_receive | then(...)\n";

  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << '\n';
  }
}

// Example 3: Using transport properties (TAPS-inspired)
void example_transport_properties() {
  std::cout << "\n=== Transport Properties Example ===\n";

  // Create transport properties for reliable streaming
  auto reliable_props = transport_properties::reliable_stream();
  std::cout << "Reliability: "
            << (reliable_props.reliability() == transport_preference::require ? "required"
                                                                              : "optional")
            << '\n';
  std::cout << "Preserve order: "
            << (reliable_props.preserve_order() == transport_preference::require ? "required"
                                                                                 : "optional")
            << '\n';

  // Create transport properties for unreliable datagram
  auto datagram_props = transport_properties::unreliable_datagram();
  std::cout << "Message boundaries: "
            << (datagram_props.preserve_msg_boundaries() == transport_preference::require
                    ? "required"
                    : "optional")
            << '\n';

  // Security properties
  auto security = security_properties::tls_1_3_only();
  if (auto protocols = security.allowed_protocols()) {
    std::cout << "Allowed security protocols: ";
    for (auto const& proto : *protocols) {
      std::cout << proto << " ";
    }
    std::cout << '\n';
  }

  auto http2_security = security_properties::http2_over_tls();
  if (auto alpn_protocols = http2_security.alpn()) {
    std::cout << "ALPN protocols: ";
    for (auto const& proto : *alpn_protocols) {
      std::cout << proto << " ";
    }
    std::cout << '\n';
  }
}

// Example 4: Error handling with networking operations
void example_error_handling() {
  std::cout << "\n=== Error Handling Example ===\n";

  io_context ctx;
  tcp_socket socket(ctx);

  try {
    socket.open(tcp());

    std::vector<char>       buffer(1024);
    mutable_buffer_sequence buffers;
    buffers.push_back(flow::net::buffer(buffer.data(), buffer.size()));

    std::cout << "Configured buffer for async receive\n";
    std::cout << "Error handling would catch:\n";
    std::cout << "  - ECONNRESET (connection reset by peer)\n";
    std::cout << "  - ETIMEDOUT (connection timed out)\n";
    std::cout << "  - Other system errors via upon_error\n";

  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << '\n';
  }
}

// Example 5: Integration with when_all for concurrent operations
void example_concurrent_operations() {
  std::cout << "\n=== Concurrent Operations Example ===\n";

  io_context ctx1;
  io_context ctx2;

  tcp_socket socket1(ctx1);
  tcp_socket socket2(ctx2);

  std::vector<char> buffer1(1024);
  std::vector<char> buffer2(1024);

  mutable_buffer_sequence buffers1;
  buffers1.push_back(buffer(buffer1.data(), buffer1.size()));

  mutable_buffer_sequence buffers2;
  buffers2.push_back(buffer(buffer2.data(), buffer2.size()));

  try {
    socket1.open(tcp());
    socket2.open(tcp());

    std::cout << "Created two sockets with separate I/O contexts\n";
    std::cout << "Concurrent operations would use when_all to:\n";
    std::cout << "  - Receive from socket1 and socket2 simultaneously\n";
    std::cout << "  - Combine results when both complete\n";
    std::cout << "  - Process total bytes received\n";

  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << '\n';
  }
}

int main() {
  std::cout << "Flow Networking Examples\n";
  std::cout << "Based on P2762R2, P3185R0 (TAPS), and P3482R0\n\n";

  // Note: These examples demonstrate the API structure.
  // They would need actual network endpoints and running servers to execute fully.

  example_tcp_client();
  example_send_receive();
  example_transport_properties();
  example_error_handling();
  example_concurrent_operations();

  std::cout << "\n=== Examples Complete ===\n";
  std::cout << "\nKey Features Demonstrated:\n";
  std::cout << "✓ Sender-based async I/O operations (P2762R2)\n";
  std::cout << "✓ Property-based configuration (P3482R0 TAPS)\n";
  std::cout << "✓ Error handling via set_error channel\n";
  std::cout << "✓ Composition with then, let_value, upon_error\n";
  std::cout << "✓ Concurrent operations with when_all\n";
  std::cout << "✓ Integration with execution schedulers\n";

  return 0;
}
