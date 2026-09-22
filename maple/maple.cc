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

#include <algorithm>
#include <array>
#include <cctype>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#if !defined(__APPLE__)
#include <asm-generic/socket.h>
#endif

#include "gemini.hh"
#include "maple.hh"
#include "titan.hh"
#include "size.hh"
#include "connection.hh"
#include "content.hh"

namespace {
auto serve_request(maple::Connection &connection, bool titan_enabled,
                   const std::string &token, std::size_t maximum_upload_bytes)
    -> std::string {
  constexpr std::string_view titan_scheme = "titan://";
  constexpr std::string_view gemini_scheme = "gemini://";
  const auto header = connection.read_header();

  if (!header) {
    return "59 Invalid request header.\r\n";
  }

  const bool is_titan = header->starts_with(titan_scheme);
  const std::size_t scheme_bytes =
      is_titan ? titan_scheme.size() : gemini_scheme.size();

  if (!is_titan && !header->starts_with(gemini_scheme)) {
    return "59 Unsupported scheme.\r\n";
  }

  const std::string_view address(*header);
  const std::size_t path_start_bytes = address.find('/', scheme_bytes);
  const std::string_view authority =
      address.substr(scheme_bytes, path_start_bytes - scheme_bytes);

  if (authority.empty() ||
      address.find_first_of(" \t\r\n#") != std::string_view::npos) {
    return "59 Invalid URL.\r\n";
  }

  const std::string path = path_start_bytes == std::string_view::npos
                               ? "/"
                               : std::string(address.substr(path_start_bytes));

  if (!is_titan) {
    std::stringstream response;
    const std::size_t query_start_bytes = path.find('?');
    const std::string content_path = path.substr(0, query_start_bytes);

    maple::gemini::handle_client(content_path, response);

    return response.str();
  }

  if (!titan_enabled) {
    return "59 Titan is disabled.\r\n";
  }

  const auto upload =
      maple::titan::parse_upload(path, token, maximum_upload_bytes);

  if (!upload) {
    return "59 Invalid Titan parameters or token.\r\n";
  }

  const auto body = connection.read_body(upload->size_bytes);

  if (!body) {
    return "59 Incomplete upload.\r\n";
  }

  if (!maple::content::write(upload->path, *body)) {
    return "50 Unable to write content.\r\n";
  }

  return "20 text/gemini\r\nUpload complete.\n";
}
} // namespace

auto main() -> int {
  bool titan_enabled = false;
  std::string titan_token;
  std::size_t maximum_upload_bytes = 0;

  if (maple::setup_environment(titan_enabled, titan_token,
                               maximum_upload_bytes) == 1) {
    return EXIT_FAILURE;
  }

  signal(SIGINT, [](int _signal) -> void {
    std::cout << "shutdown(" << _signal << ")\n";
    close(maple::maple_socket);
    SSL_CTX_free(maple::ssl_context);
  });
  signal(SIGPIPE, SIG_IGN);

  if (maple::setup_ssl() == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  for (;;) {
    sockaddr_in socket_address{};
    unsigned int socket_address_bytes = sizeof(socket_address);
    SSL *tls_session;
    const int client = accept(maple::maple_socket,
                              reinterpret_cast<sockaddr *>(&socket_address),
                              &socket_address_bytes);

    if (client < 0) {
      return maple::prepare_exit_with("unable to accept", false);
    }

    tls_session = SSL_new(maple::ssl_context);

    SSL_set_fd(tls_session, client);

    if (SSL_accept(tls_session) <= 0) {
      ERR_print_errors_fp(stderr);
    } else {
      maple::Connection connection(tls_session);
      const std::string response = serve_request(
          connection, titan_enabled, titan_token, maximum_upload_bytes);

      if (!connection.write(response)) {
        ERR_print_errors_fp(stderr);
      }
    }

    SSL_shutdown(tls_session);
    SSL_free(tls_session);
    close(client);
  }
}

namespace maple {
auto prepare_exit_with(const char *message, bool ssl) -> int {
  perror(message);

  if (ssl) {
    ERR_print_errors_fp(stderr);
  }

  return EXIT_FAILURE;
}

auto setup_environment(bool &titan_enabled, std::string &titan_token,
                       std::size_t &maximum_upload_bytes) -> int {
  char *titan_environment = std::getenv("TITAN");

  if (titan_environment == nullptr) {
    titan_enabled = false;
  } else {
    std::string valid_titan_environment(titan_environment);

    std::transform(
        valid_titan_environment.begin(), valid_titan_environment.end(),
        valid_titan_environment.begin(),
        [](unsigned char character) -> int { return std::tolower(character); });

    if (valid_titan_environment == "true" || valid_titan_environment == "1") {
      char *unvalidated_titan_token = std::getenv("TITAN_TOKEN");
      char *configured_maximum_upload_bytes = std::getenv("TITAN_MAX_SIZE");

      if (unvalidated_titan_token == nullptr) {
        titan_token = "";
      } else {
        titan_token = std::string(unvalidated_titan_token);
      }

      if (configured_maximum_upload_bytes == nullptr) {
        constexpr std::size_t default_maximum_upload_bytes = 1024;

        maximum_upload_bytes = default_maximum_upload_bytes;

        std::cout << "no TITAN_MAX_SIZE set, defaulting to 1024\n";
      } else {
        const auto parsed_maximum_upload_bytes =
            maple::parse_size(configured_maximum_upload_bytes);

        if (!parsed_maximum_upload_bytes) {
          std::cerr << "TITAN_MAX_SIZE must be an unsigned integer.\n";

          return EXIT_FAILURE;
        }

        maximum_upload_bytes = *parsed_maximum_upload_bytes;
      }

      titan_enabled = true;
    }
  }

  return 0;
}

auto setup_ssl() -> int {
  sockaddr_in socket_address{};

  // Setup OpenSSL
  SSL_library_init();
  SSL_load_error_strings();

  maple::ssl_context = SSL_CTX_new(TLS_server_method());

  if (maple::ssl_context == nullptr) {
    return maple::prepare_exit_with("unable to create ssl context", true);
  }

  if (SSL_CTX_use_certificate_file(maple::ssl_context, ".maple/public.pem",
                                   SSL_FILETYPE_PEM) <= 0) {
    return maple::prepare_exit_with("unable to use certificate file", true);
  }

  if (SSL_CTX_use_PrivateKey_file(maple::ssl_context, ".maple/private.pem",
                                  SSL_FILETYPE_PEM) <= 0) {
    return maple::prepare_exit_with("unable to use private key file", true);
  }

  constexpr std::size_t GEMINI_PORT = 1965;

  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(GEMINI_PORT);
  socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
  maple::maple_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (maple::maple_socket < 0) {
    return maple::prepare_exit_with("unable to create socket", false);
  }

  // Reuse address. Allows the use of the address instantly after a SIGINT
  // without having to wait for the socket to die.
  int reuse_addr = 1;

  if (setsockopt(maple::maple_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                 sizeof(int)) < 0) {
    return maple::prepare_exit_with("unable to set socket options (SO_LINGER)",
                                    false);
  }

  if (bind(maple::maple_socket, reinterpret_cast<sockaddr *>(&socket_address),
           sizeof(socket_address)) < 0) {
    return maple::prepare_exit_with("unable to bind", false);
  }

  if (listen(maple::maple_socket, 1) < 0) {
    return maple::prepare_exit_with("unable to listen", false);
  }

  return 0;
}
} // namespace maple
