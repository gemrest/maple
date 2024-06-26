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
#include <asm-generic/socket.h>
#include <cctype>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "gemini.hh"
#include "maple.hh"
#include "titan.hh"

auto main() -> int {
  std::vector<std::string> gemini_files;
  bool titan = false;
  std::string titan_token;
  std::size_t titan_max_size = 0;
  const std::string GEMINI_FILE_EXTENSION = "gmi";

  // Check if the user is want to support Titan and set it up
  if (maple::setup_environment(titan, titan_token, titan_max_size) == 1) {
    return EXIT_FAILURE;
  }

  // Try a graceful shutdown when a SIGINT is detected
  signal(SIGINT, [](int _signal) -> void {
    std::cout << "shutdown(" << _signal << ")\n";

    close(maple::maple_socket);
    SSL_CTX_free(maple::ssl_context);
  });

  // Find and keep track of all Gemini files to serve
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(".maple/gmi")) {
    std::string file_extension = entry.path().string().substr(
        entry.path().string().find_last_of('.') + 1);

    // Only keep track of file if it is a Gemini file
    if (std::equal(file_extension.begin(), file_extension.end(),
                   GEMINI_FILE_EXTENSION.begin(), GEMINI_FILE_EXTENSION.end(),
                   [](char character_a, char character_b) -> bool {
                     return std::tolower(character_a) ==
                            std::tolower(character_b);
                   })) {
      gemini_files.push_back(entry.path());
    }
  }

  // Inform user of which files will be served
  for (const std::string &file : gemini_files) {
    std::cout << "serving " << file << '\n';
  }

  // Setup SSL
  if (maple::setup_ssl() == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  // Listen and serve connections
  for (;;) {
    sockaddr_in socket_address_{};
    unsigned int socket_address_length = sizeof(socket_address_);
    SSL *ssl;
    const int client = accept(maple::maple_socket,
                              reinterpret_cast<sockaddr *>(&socket_address_),
                              &socket_address_length);

    if (client < 0) {
      return maple::prepare_exit_with("unable to accept", false);
    }

    ssl = SSL_new(maple::ssl_context);

    SSL_set_fd(ssl, client);

    if (SSL_accept(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
    } else {
      std::stringstream response;
      std::size_t index_of_junk;
      int request_scheme; // Gemini = 1, Titan = 2, Error = 0
      std::size_t bytes_read;
      constexpr std::size_t GEMINI_MAXIMUM_REQUEST_SIZE = 1024;
      std::array<char, GEMINI_MAXIMUM_REQUEST_SIZE> request{};

      SSL_read_ex(ssl, request.begin(), request.size(), &bytes_read);

      std::string path(request.data());

      if (path.starts_with("gemini://")) {
        request_scheme = 1;
      } else if (path.starts_with("titan://")) {
        request_scheme = 2;
      } else {
        request_scheme = 0;
      }

      if (request_scheme != 0) {
        path = path.substr(0, bytes_read);

        // Remove "\r\n" if Gemini
        if (request_scheme == 1) {
          path = path.substr(0, path.size() - 2);
        }

        if (request_scheme == 1) {
          constexpr std::size_t GEMINI_PROTOCOL_LENGTH = 9;

          path.erase(0, GEMINI_PROTOCOL_LENGTH); // Remove "gemini://"
        } else {
          constexpr std::size_t TITAN_PROTOCOL_LENGTH = 8;

          path.erase(0, TITAN_PROTOCOL_LENGTH); // Remove "titan://"
        }

        // Try to remove the host, if you cannot; it must be a trailing
        // slash-less hostname, so we will respond with the index.
        const std::size_t found_first = path.find_first_of('/');

        if (found_first != std::string::npos) {
          path = path.substr(found_first,
                             path.size() - 1); // Remove host
        } else {
          path = "/index.gmi";
        }

        if (request_scheme == 1) {
          // Remove junk, if any
          index_of_junk = path.find_first_of('\n');

          if (index_of_junk != std::string::npos) {
            path.erase(path.find_first_of('\n') - 1, path.size() - 1);
          }
        }

        // Gemini
        if (request_scheme == 1) {
          maple::gemini::handle_client(gemini_files, path, response);
        } else { // Titan
          if (!titan) {
            response << "20 text/gemini\r\nThe server (Maple) does not have "
                        "Titan support enabled!";
          } else {
            maple::titan::handle_client(response, path, titan_token,
                                        titan_max_size);
          }
        }

        SSL_write(ssl, response.str().c_str(),
                  static_cast<int>(response.str().size()));
      } else {
        std::cout << "received a request with an unsupported url scheme\n";
      }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
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

auto setup_environment(bool &titan, std::string &titan_token,
                       std::size_t &titan_max_size) -> int {
  char *titan_environment = std::getenv("TITAN");

  if (titan_environment == nullptr) {
    titan = false;
  } else {
    std::string valid_titan_environment(titan_environment);

    std::transform(
        valid_titan_environment.begin(), valid_titan_environment.end(),
        valid_titan_environment.begin(),
        [](unsigned char character) -> int { return std::tolower(character); });

    if (valid_titan_environment == "true" || valid_titan_environment == "1") {
      char *unvalidated_titan_token = std::getenv("TITAN_TOKEN");
      char *unvalidated_titan_max_size = std::getenv("TITAN_MAX_SIZE");

      if (unvalidated_titan_token == nullptr) {
        titan_token = "";
      } else {
        titan_token = std::string(unvalidated_titan_token);
      }

      if (unvalidated_titan_max_size == nullptr) {
        constexpr std::size_t TITAN_MAX_SIZE = 1024;

        titan_max_size = TITAN_MAX_SIZE;

        std::cout << "no TITAN_MAX_SIZE set, defaulting to 1024\n";
      } else {
        try {
          titan_max_size =
              static_cast<std::size_t>(std::stoi(unvalidated_titan_max_size));
        } catch (...) {
          return maple::prepare_exit_with(
              "TITAN_MAX_SIZE could not be interpreted as an integer", false);
        }
      }

      titan = true;
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
