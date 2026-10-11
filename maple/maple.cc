/*
 * This file is part of Maple <https://github.com/gemrest/maple>.
 * Copyright (C) 2022-2022 Fuwn <contact@fuwn.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2022-2022 Fuwn <contact@fuwn.me>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "connection.hh"
#include "content.hh"
#include "gemini.hh"
#include "size.hh"
#include "titan.hh"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <memory>
#include <netinet/in.h>
#include <openssl/err.h>
#include <poll.h>
#include <sys/socket.h>

namespace {
volatile std::sig_atomic_t stopping = 0;

struct Configuration {
  std::uint16_t port = 1965;
  bool titan_enabled = false;
  std::string token;
  std::size_t maximum_upload_bytes = 1024;
};

auto configuration() -> std::optional<Configuration> {
  Configuration settings;

  if (const char *configured_port = std::getenv("MAPLE_PORT")) {
    const auto port = maple::parse_size(configured_port);

    if (!port || *port == 0 || *port > std::numeric_limits<std::uint16_t>::max()) {
      std::cerr << "MAPLE_PORT must be an unsigned integer between 1 and 65535.\n";

      return std::nullopt;
    }

    settings.port = static_cast<std::uint16_t>(*port);
  }

  const char *enabled = std::getenv("TITAN");

  if (enabled == nullptr) {
    return settings;
  }

  std::string value(enabled);

  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });

  settings.titan_enabled = value == "1" || value == "true";

  if (!settings.titan_enabled) {
    return settings;
  }

  if (const char *token = std::getenv("TITAN_TOKEN")) {
    settings.token = token;
  }

  if (const char *configured_maximum_upload_bytes =
          std::getenv("TITAN_MAX_SIZE")) {
    const auto parsed_maximum_upload_bytes =
        maple::parse_size(configured_maximum_upload_bytes);

    if (!parsed_maximum_upload_bytes) {
      std::cerr << "TITAN_MAX_SIZE must be an unsigned integer.\n";

      return std::nullopt;
    }

    settings.maximum_upload_bytes = *parsed_maximum_upload_bytes;
  }

  return settings;
}

auto serve_request(maple::Connection &connection, bool titan_enabled,
                   const std::string &token, std::size_t maximum_upload_bytes)
    -> bool {
  constexpr std::string_view titan_scheme = "titan://";
  constexpr std::string_view gemini_scheme = "gemini://";
  const auto header = connection.read_header();

  if (!header) {
    return connection.write("59 Invalid request header.\r\n");
  }

  const bool is_titan = header->starts_with(titan_scheme);
  const std::size_t scheme_bytes =
      is_titan ? titan_scheme.size() : gemini_scheme.size();

  if (!is_titan && !header->starts_with(gemini_scheme)) {
    return connection.write("59 Unsupported scheme.\r\n");
  }

  const std::string_view address(*header);
  const std::size_t path_start_bytes = address.find('/', scheme_bytes);
  const std::string_view authority =
      address.substr(scheme_bytes, path_start_bytes - scheme_bytes);

  if (authority.empty() ||
      address.find_first_of(" \t\r\n#") != std::string_view::npos) {
    return connection.write("59 Invalid URL.\r\n");
  }

  const std::string path = path_start_bytes == std::string_view::npos
                               ? "/"
                               : std::string(address.substr(path_start_bytes));

  if (!is_titan) {
    const std::size_t query_start_bytes = path.find('?');
    const std::string content_path = path.substr(0, query_start_bytes);

    return maple::gemini::handle_client(connection, content_path);
  }

  if (!titan_enabled) {
    return connection.write("59 Titan is disabled.\r\n");
  }

  const auto upload =
      maple::titan::parse_upload(path, token, maximum_upload_bytes);

  if (!upload) {
    return connection.write("59 Invalid Titan parameters or token.\r\n");
  }

  const auto body = connection.read_body(upload->size_bytes);

  if (!body) {
    return connection.write("59 Incomplete upload.\r\n");
  }

  if (!maple::content::write(upload->path, *body)) {
    return connection.write("50 Unable to write content.\r\n");
  }

  return connection.write("20 text/gemini\r\nUpload complete.\n");
}

auto listen_socket(std::uint16_t port) -> maple::Descriptor {
  maple::Descriptor listener(socket(AF_INET, SOCK_STREAM, 0));
  sockaddr_in address{};
  const int reuse_address = 1;

  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (listener.get() < 0 ||
      setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                 sizeof(reuse_address)) < 0 ||
      fcntl(listener.get(), F_SETFL, O_NONBLOCK) < 0 ||
      bind(listener.get(), reinterpret_cast<sockaddr *>(&address),
           sizeof(address)) < 0 ||
      listen(listener.get(), SOMAXCONN) < 0) {
    perror("Unable to listen on the configured port");

    return maple::Descriptor(-1);
  }

  return listener;
}
} // namespace

auto main() -> int {
  const auto settings = configuration();

  if (!settings) {
    return EXIT_FAILURE;
  }

  struct sigaction action{};

  action.sa_handler = [](int) { stopping = 1; };

  sigemptyset(&action.sa_mask);

  if (sigaction(SIGINT, &action, nullptr) < 0 ||
      sigaction(SIGTERM, &action, nullptr) < 0) {
    perror("Unable to install shutdown handlers");

    return EXIT_FAILURE;
  }

  action.sa_handler = SIG_IGN;

  if (sigaction(SIGPIPE, &action, nullptr) < 0) {
    perror("Unable to ignore SIGPIPE");

    return EXIT_FAILURE;
  }

  const std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context(
      SSL_CTX_new(TLS_server_method()), SSL_CTX_free);

  if (!tls_context ||
      SSL_CTX_use_certificate_file(tls_context.get(), ".maple/public.pem",
                                   SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_use_PrivateKey_file(tls_context.get(), ".maple/private.pem",
                                  SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_check_private_key(tls_context.get()) != 1) {
    std::cerr
        << "Unable to load the server certificate and private key from .maple.\n";
    ERR_print_errors_fp(stderr);

    return EXIT_FAILURE;
  }

  const auto listener = listen_socket(settings->port);

  if (listener.get() < 0) {
    return EXIT_FAILURE;
  }

  while (!stopping) {
    constexpr int connection_poll_interval_milliseconds = 250;
    pollfd descriptor{listener.get(), POLLIN, 0};
    const int ready =
        poll(&descriptor, 1, connection_poll_interval_milliseconds);

    if (ready < 0 && errno != EINTR) {
      perror("Unable to wait for a connection");

      return EXIT_FAILURE;
    }

    if (ready <= 0 || stopping) {
      continue;
    }

    const int client = accept(listener.get(), nullptr, nullptr);

    if (client < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ||
          errno == ECONNABORTED) {
        continue;
      }

      perror("Unable to accept a connection");

      return EXIT_FAILURE;
    }

    maple::Connection connection(tls_context.get(), client, stopping);

    if (connection.handshake()) {
      serve_request(connection, settings->titan_enabled, settings->token,
                    settings->maximum_upload_bytes);
    }
  }

  return EXIT_SUCCESS;
}
