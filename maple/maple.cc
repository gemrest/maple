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

#include <arpa/inet.h>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static int maple_socket;
static SSL_CTX *ssl_context;

auto exit_with[[noreturn]](const char *, bool) -> void;

auto main() -> int {
  sockaddr_in socket_address {};
  std::vector<std::string> gemini_files;
  const std::string GEMINI_FILE_EXTENSION = "gmi";

  // Try a graceful shutdown when a SIGINT is detected
  signal(SIGINT, [](int signal_) -> void {
    std::cout << "shutdown(" << signal_ << ")" << std::endl;

    close(maple_socket);
    SSL_CTX_free(ssl_context);
  });

  // Find and keep track of all Gemini files to serve
  for (const std::filesystem::directory_entry &entry :
    std::filesystem::recursive_directory_iterator(".maple/gmi")) {

    std::string file_extension = entry.path().string().substr(
      entry.path().string().find_last_of('.') + 1
    );

    // Only keep track of file if it is a Gemini file
    if (std::equal(
      file_extension.begin(),
      file_extension.end(),
      GEMINI_FILE_EXTENSION.begin(),
      GEMINI_FILE_EXTENSION.end(),
      [](char a, char b) -> bool { return std::tolower(a) == std::tolower(b); }
    )) { gemini_files.push_back(entry.path()); }
  }

  // Inform user of which files will be served
  for (const std::string &file : gemini_files) {
    std::cout << "serving " << file << std::endl;
  }

  // Setup OpenSSL
  SSL_library_init();
  SSL_load_error_strings();

  ssl_context = SSL_CTX_new(TLS_server_method());
  if (!ssl_context) { exit_with("unable to create ssl context", true); }

  if (SSL_CTX_use_certificate_file(
    ssl_context,
    ".maple/public.pem",
    SSL_FILETYPE_PEM
  ) <= 0) { exit_with("unable to use certificate file", true); }
  if (SSL_CTX_use_PrivateKey_file(
    ssl_context,
    ".maple/private.pem",
    SSL_FILETYPE_PEM
  ) <= 0) { exit_with("unable to use private key file", true); }

  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(1965);
  socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

  maple_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (maple_socket < 0) { exit_with("unable to create socket", false); }

  // Reuse address. Allows the use of the address instantly after a SIGINT
  // without having to wait for the socket to die.
  int reuse_addr = 1;
  if (setsockopt(
    maple_socket,
    SOL_SOCKET,
    SO_REUSEADDR,
    &reuse_addr,
    sizeof(int)
  ) < 0) { exit_with("unable to set socket options (SO_LINGER)", false); }

  if (bind(
    maple_socket,
    reinterpret_cast<sockaddr *>(&socket_address),
    sizeof(socket_address)
  ) < 0) { exit_with("unable to bind", false); }
  if (listen(maple_socket, 1) < 0) { exit_with("unable to listen", false); }

  // Listen and serve connections
  for (;;) {
    sockaddr_in socket_address_ {};
    unsigned int socket_address_length = sizeof(socket_address_);
    SSL *ssl;
    int client = accept(
      maple_socket,
      reinterpret_cast<sockaddr *>(&socket_address_),
      &socket_address_length
    );
    char request[1024];

    if (client < 0) { exit_with("unable to accept", false); }

    ssl = SSL_new(ssl_context);
    SSL_set_fd(ssl, client);

    if (SSL_accept(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
    } else {
      std::stringstream response;
      size_t index_of_junk;

      SSL_read(ssl, request, sizeof(request));

      std::string path(request);

      path = path.substr(0, path.size() - 2); // Remove "\r\n"
      path.erase(0, 9); // Remove "gemini://"

      // Try to remove the host, if you cannot; it must be a trailing slash-less
      // hostname, so we will respond with the index.
      size_t found_first = path.find_first_of('/');
      if (found_first != std::string::npos) {
        path = path.substr(found_first, path.size() - 1); // Remove host
      } else {
        path = "/index.gmi";
      }

      // Remove junk, if any
      index_of_junk = path.find_first_of('\n');
      if (index_of_junk != std::string::npos) {
        path.erase(path.find_first_of('\n') - 1, path.size() - 1);
      }

      // Check if the route is a file being served
      if (std::find(
        gemini_files.begin(),
        gemini_files.end(),
        ".maple/gmi" + path
      ) != gemini_files.end()) {
        // If the route is a file being served; get the file contents

        std::ifstream file(".maple/gmi" + path);
        std::stringstream buffer;

        buffer << file.rdbuf();

        response << "20 text/gemini\r\n" << buffer.str();
      } else {
        if (path.empty() || path.at(path.length() - 1) == '/') {
          std::ifstream file(".maple/gmi" + path + "index.gmi");
          std::stringstream buffer;

          buffer << file.rdbuf();

          response << "20 text/gemini\r\n" << buffer.str();
        } else {
          response << "51 The server (Maple) could not find the specified file.\r\n";
        }
      }

      std::cout << "requested " << path << std::endl;

      SSL_write(ssl, response.str().c_str(), static_cast<int>(response.str().size()));
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client);
  }
}

auto exit_with[[noreturn]](const char *message, bool ssl) -> void {
  perror(message);
  if (ssl) { ERR_print_errors_fp(stderr); }
  std::exit(EXIT_FAILURE);
}
