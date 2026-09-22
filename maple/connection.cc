#include "connection.hh"
#include <algorithm>
#include <array>
#include <openssl/err.h>
#include <poll.h>

namespace maple {
auto Connection::record_progress() -> void {
  if (transferring_) {
    deadline_ = std::chrono::steady_clock::now() + transfer_timeout;
  }
}

auto Connection::retry(int result) -> bool {
  constexpr int maximum_poll_interval_milliseconds = 250;
  const int error = SSL_get_error(tls_session_, result);

  if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
    return false;
  }

  const auto remaining = deadline_ - std::chrono::steady_clock::now();

  if (remaining <= std::chrono::steady_clock::duration::zero()) {
    return false;
  }

  const auto milliseconds =
      std::chrono::ceil<std::chrono::milliseconds>(remaining);
  pollfd descriptor{
      SSL_get_fd(tls_session_),
      static_cast<short>(error == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT), 0};

  // Poll's writability threshold can exceed the space needed for a small write.
  const auto wait_duration =
      error == SSL_ERROR_WANT_WRITE
          ? std::min(milliseconds, std::chrono::milliseconds(
                                       maximum_poll_interval_milliseconds))
          : milliseconds;
  const int ready =
      poll(&descriptor, 1, static_cast<int>(wait_duration.count()));

  return (ready > 0 && (descriptor.revents & descriptor.events) != 0) ||
         (ready == 0 && error == SSL_ERROR_WANT_WRITE);
}

auto Connection::handshake() -> bool {
  while (std::chrono::steady_clock::now() < deadline_) {
    ERR_clear_error();

    const int result = SSL_accept(tls_session_);

    if (result == 1) {
      return true;
    }

    if (!retry(result)) {
      return false;
    }
  }

  return false;
}

auto Connection::receive(char *buffer, std::size_t size_bytes,
                         std::size_t &byte_count) -> bool {
  while (std::chrono::steady_clock::now() < deadline_) {
    ERR_clear_error();

    const int result =
        SSL_read_ex(tls_session_, buffer, size_bytes, &byte_count);

    if (result == 1) {
      if (byte_count == 0) {
        return false;
      }

      record_progress();

      return true;
    }

    if (!retry(result)) {
      return false;
    }
  }

  return false;
}

auto Connection::read_header() -> std::optional<std::string> {
  constexpr std::size_t maximum_url_bytes = 1024;
  constexpr std::size_t header_terminator_bytes = 2;
  std::string header;

  while (header.size() < maximum_url_bytes + header_terminator_bytes) {
    char character = '\0';
    std::size_t byte_count = 0;

    if (!receive(&character, 1, byte_count)) {
      return std::nullopt;
    }

    if (character == '\n') {
      if (header.empty() || header.back() != '\r') {
        return std::nullopt;
      }

      header.pop_back();

      transferring_ = true;

      record_progress();

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

    if (!receive(buffer.data(), remaining_bytes, byte_count)) {
      return std::nullopt;
    }

    body.append(buffer.data(), byte_count);
  }

  return body;
}

auto Connection::write(std::string_view response) -> bool {
  while (!response.empty()) {
    std::size_t byte_count = 0;

    if (std::chrono::steady_clock::now() >= deadline_) {
      return false;
    }

    ERR_clear_error();

    const std::size_t write_bytes =
        std::min(response.size(), maximum_write_bytes);
    const int result =
        SSL_write_ex(tls_session_, response.data(), write_bytes, &byte_count);

    if (result != 1) {
      if (!retry(result)) {
        return false;
      }

      continue;
    }

    if (byte_count == 0) {
      return false;
    }

    response.remove_prefix(byte_count);
    record_progress();
  }

  return true;
}
} // namespace maple
