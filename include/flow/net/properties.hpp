#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace flow::net {

// [net.properties] Network property types
// Based on P3482R0 - TAPS property system

// Transport preference enumeration (P3482R0 section 4.13.1)
enum class transport_preference {
  require,   // Must have this property
  prefer,    // Would like to have this property
  none,      // No preference
  avoid,     // Would prefer not to have this property
  prohibit,  // Must not have this property
};

// Multipath preference (P3482R0 section 4.13.2)
enum class multipath_preference {
  disabled,  // No multipath
  active,    // Actively use multipath
  passive,   // Passively support multipath
};

// Direction preference (P3482R0 section 4.13.3)
enum class direction_preference {
  bidirectional,  // Both send and receive
  send,           // Send only
  recv,           // Receive only
};

// Endpoint properties namespace (P3482R0 section 4.12)
namespace endpoint_props {

struct hostname {
  std::string value;

  explicit hostname(std::string_view h) : value(h) {}
};

struct port {
  std::uint16_t value;

  explicit constexpr port(std::uint16_t p) noexcept : value(p) {}
};

struct service {
  std::string value;

  explicit service(std::string_view s) : value(s) {}

  // Well-known services
  static service https() {
    return service("https");
  }
  static service http() {
    return service("http");
  }
  static service ftp() {
    return service("ftp");
  }
  static service ssh() {
    return service("ssh");
  }
};

struct interface_name {
  std::string value;

  explicit interface_name(std::string_view i) : value(i) {}
};

}  // namespace endpoint_props

// Transport properties namespace (P3482R0 section 4.13)
namespace transport_props {

struct reliability {
  transport_preference value = transport_preference::require;
};

struct preserve_msg_boundaries {
  transport_preference value = transport_preference::none;
};

struct per_msg_reliability {
  transport_preference value = transport_preference::none;
};

struct preserve_order {
  transport_preference value = transport_preference::require;
};

struct zero_rtt_msg {
  transport_preference value = transport_preference::none;
};

struct multistreaming {
  transport_preference value = transport_preference::prefer;
};

struct full_checksum_send {
  transport_preference value = transport_preference::require;
};

struct full_checksum_recv {
  transport_preference value = transport_preference::require;
};

struct congestion_control {
  transport_preference value = transport_preference::require;
};

struct keep_alive {
  transport_preference value = transport_preference::none;
};

struct multipath {
  multipath_preference value = multipath_preference::disabled;
};

struct direction {
  direction_preference value = direction_preference::bidirectional;
};

}  // namespace transport_props

// Security properties namespace (P3482R0 section 4.14)
namespace security_props {

struct allowed_protocols {
  std::vector<std::string> value;

  allowed_protocols() = default;
  explicit allowed_protocols(std::vector<std::string> protocols) : value(std::move(protocols)) {}

  // Common protocols
  static allowed_protocols tls_1_3() {
    return allowed_protocols({{"TLSv1.3"}});
  }
  static allowed_protocols tls_1_2_and_1_3() {
    return allowed_protocols({{"TLSv1.2", "TLSv1.3"}});
  }
};

struct server_certificate {
  std::vector<std::string> value;  // PEM-encoded certificates
};

struct client_certificate {
  std::vector<std::string> value;  // PEM-encoded certificates
};

struct pinned_server_certificate {
  std::vector<std::string> value;  // PEM-encoded pinned certificates
};

// Application Layer Protocol Negotiation (RFC 7301)
struct alpn {
  std::vector<std::string> value;

  alpn() = default;
  explicit alpn(std::vector<std::string> protocols) : value(std::move(protocols)) {}

  // Common ALPN values
  static alpn http_1_1() {
    return alpn({{"http/1.1"}});
  }
  static alpn http_2() {
    return alpn({{"h2"}});
  }
  static alpn http_3() {
    return alpn({{"h3"}});
  }
};

struct supported_groups {
  std::vector<std::string> value;  // e.g., "secp256r1", "x25519"
};

struct ciphersuites {
  std::vector<std::string> value;
};

struct max_cached_sessions {
  std::uint32_t value = 100;
};

struct cached_session_lifetime {
  std::chrono::steady_clock::duration value = std::chrono::hours(24);
};

}  // namespace security_props

// Property container for preconnection (simplified version of P3482 runtime_env)
class transport_properties {
 public:
  transport_properties() = default;

  // Transport property setters
  void set_reliability(transport_preference pref) {
    reliability_ = pref;
  }
  void set_preserve_msg_boundaries(transport_preference pref) {
    preserve_msg_boundaries_ = pref;
  }
  void set_preserve_order(transport_preference pref) {
    preserve_order_ = pref;
  }
  void set_multistreaming(transport_preference pref) {
    multistreaming_ = pref;
  }
  void set_congestion_control(transport_preference pref) {
    congestion_control_ = pref;
  }
  void set_keep_alive(transport_preference pref) {
    keep_alive_ = pref;
  }
  void set_multipath(multipath_preference pref) {
    multipath_ = pref;
  }
  void set_direction(direction_preference pref) {
    direction_ = pref;
  }

  // Transport property getters
  [[nodiscard]] transport_preference reliability() const noexcept {
    return reliability_;
  }
  [[nodiscard]] transport_preference preserve_msg_boundaries() const noexcept {
    return preserve_msg_boundaries_;
  }
  [[nodiscard]] transport_preference preserve_order() const noexcept {
    return preserve_order_;
  }
  [[nodiscard]] transport_preference multistreaming() const noexcept {
    return multistreaming_;
  }
  [[nodiscard]] transport_preference congestion_control() const noexcept {
    return congestion_control_;
  }
  [[nodiscard]] transport_preference keep_alive() const noexcept {
    return keep_alive_;
  }
  [[nodiscard]] multipath_preference multipath() const noexcept {
    return multipath_;
  }
  [[nodiscard]] direction_preference direction() const noexcept {
    return direction_;
  }

  // Static factory methods for common configurations
  static transport_properties reliable_stream() {
    transport_properties props;
    props.set_reliability(transport_preference::require);
    props.set_preserve_order(transport_preference::require);
    props.set_congestion_control(transport_preference::require);
    return props;
  }

  static transport_properties unreliable_datagram() {
    transport_properties props;
    props.set_reliability(transport_preference::prohibit);
    props.set_preserve_msg_boundaries(transport_preference::require);
    return props;
  }

 private:
  transport_preference reliability_             = transport_preference::require;
  transport_preference preserve_msg_boundaries_ = transport_preference::none;
  transport_preference preserve_order_          = transport_preference::require;
  transport_preference multistreaming_          = transport_preference::prefer;
  transport_preference congestion_control_      = transport_preference::require;
  transport_preference keep_alive_              = transport_preference::none;
  multipath_preference multipath_               = multipath_preference::disabled;
  direction_preference direction_               = direction_preference::bidirectional;
};

// Security property container
class security_properties {
 public:
  security_properties() = default;

  // Security property setters
  void set_allowed_protocols(std::vector<std::string> protocols) {
    allowed_protocols_ = std::move(protocols);
  }

  void set_alpn(std::vector<std::string> protocols) {
    alpn_ = std::move(protocols);
  }

  void set_server_certificate(std::vector<std::string> certs) {
    server_cert_ = std::move(certs);
  }

  void set_client_certificate(std::vector<std::string> certs) {
    client_cert_ = std::move(certs);
  }

  void set_max_cached_sessions(std::uint32_t max_sessions) {
    max_cached_sessions_ = max_sessions;
  }

  // Security property getters
  [[nodiscard]] std::optional<std::vector<std::string>> const& allowed_protocols() const noexcept {
    return allowed_protocols_;
  }

  [[nodiscard]] std::optional<std::vector<std::string>> const& alpn() const noexcept {
    return alpn_;
  }

  [[nodiscard]] std::optional<std::vector<std::string>> const& server_certificate() const noexcept {
    return server_cert_;
  }

  [[nodiscard]] std::optional<std::vector<std::string>> const& client_certificate() const noexcept {
    return client_cert_;
  }

  [[nodiscard]] std::uint32_t max_cached_sessions() const noexcept {
    return max_cached_sessions_;
  }

  // Static factory methods for common configurations
  static security_properties tls_1_3_only() {
    security_properties props;
    props.set_allowed_protocols({"TLSv1.3"});
    return props;
  }

  static security_properties http2_over_tls() {
    security_properties props;
    props.set_allowed_protocols({"TLSv1.2", "TLSv1.3"});
    props.set_alpn({"h2"});
    return props;
  }

 private:
  std::optional<std::vector<std::string>> allowed_protocols_;
  std::optional<std::vector<std::string>> alpn_;
  std::optional<std::vector<std::string>> server_cert_;
  std::optional<std::vector<std::string>> client_cert_;
  std::uint32_t                           max_cached_sessions_ = 100;
};

}  // namespace flow::net
