#ifndef MAPLE_CONNECTION_HH
#define MAPLE_CONNECTION_HH

#include <cstddef>
#include <openssl/ssl.h>
#include <optional>
#include <string>
#include <string_view>

namespace maple {
class Connection {
public:
  explicit Connection(SSL *tls_session) : tls_session_(tls_session) {}

  auto read_header() -> std::optional<std::string>;
  auto read_body(std::size_t size_bytes) -> std::optional<std::string>;
  auto write(std::string_view response) -> bool;

private:
  SSL *tls_session_;
};
} // namespace maple

#endif
