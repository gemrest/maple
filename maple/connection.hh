#ifndef MAPLE_CONNECTION_HH
#define MAPLE_CONNECTION_HH

#include "descriptor.hh"
#include <chrono>
#include <csignal>
#include <memory>
#include <cstddef>
#include <openssl/ssl.h>
#include <optional>
#include <string>
#include <string_view>

namespace maple {
class Connection {
public:
  Connection(SSL_CTX *tls_context, int socket,
             const volatile std::sig_atomic_t &stopping)
      : socket_(socket), tls_session_(SSL_new(tls_context), SSL_free),
        stopping_(stopping),
        deadline_(std::chrono::steady_clock::now() + request_timeout) {}

  ~Connection();
  auto handshake() -> bool;
  auto read_header() -> std::optional<std::string>;
  auto read_body(std::size_t size_bytes) -> std::optional<std::string>;
  auto write(std::string_view response) -> bool;

private:
  auto can_continue() const -> bool;
  auto record_progress() -> void;
  auto retry(int result) -> bool;
  auto receive(char *buffer, std::size_t size_bytes, std::size_t &byte_count)
      -> bool;

  static constexpr std::chrono::seconds request_timeout{5};
  static constexpr std::chrono::seconds transfer_timeout{15};
  static constexpr std::size_t maximum_write_bytes = 8192;
  Descriptor socket_;
  std::unique_ptr<SSL, decltype(&SSL_free)> tls_session_;
  const volatile std::sig_atomic_t &stopping_;
  std::chrono::steady_clock::time_point deadline_;
  bool transferring_ = false;
};
} // namespace maple

#endif
