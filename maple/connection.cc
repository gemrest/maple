#include "connection.hh"
#include <algorithm>
#include <array>

namespace maple {
auto Connection::read_header() -> std::optional<std::string> {
  constexpr std::size_t maximum_url_bytes = 1024;
  constexpr std::size_t header_terminator_bytes = 2;
  std::string header;

  while (header.size() < maximum_url_bytes + header_terminator_bytes) {
    char character = '\0';
    std::size_t byte_count = 0;

    if (SSL_read_ex(tls_session_, &character, 1, &byte_count) != 1 ||
        byte_count != 1) {
      return std::nullopt;
    }

    if (character == '\n') {
      if (header.empty() || header.back() != '\r') {
        return std::nullopt;
      }

      header.pop_back();

      return header;
    }

    if (character == '\0' || (!header.empty() && header.back() == '\r')) {
      return std::nullopt;
    }

    header += character;
  }

  return std::nullopt;
}

auto Connection::read_body(std::size_t size_bytes)
    -> std::optional<std::string> {
  constexpr std::size_t buffer_capacity_bytes = 8192;
  std::string body;
  std::array<char, buffer_capacity_bytes> buffer{};

  while (body.size() < size_bytes) {
    std::size_t byte_count = 0;
    const std::size_t remaining_bytes =
        std::min(buffer.size(), size_bytes - body.size());

    if (SSL_read_ex(tls_session_, buffer.data(), remaining_bytes, &byte_count) != 1 ||
        byte_count == 0) {
      return std::nullopt;
    }

    body.append(buffer.data(), byte_count);
  }

  return body;
}

auto Connection::write(std::string_view response) -> bool {
  while (!response.empty()) {
    std::size_t byte_count = 0;

    if (SSL_write_ex(tls_session_, response.data(), response.size(),
                     &byte_count) != 1 ||
        byte_count == 0) {
      return false;
    }

    response.remove_prefix(byte_count);
  }

  return true;
}
} // namespace maple
