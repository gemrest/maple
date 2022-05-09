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

#ifndef MAPLE_HH
#define MAPLE_HH

#include <openssl/ssl.h>

namespace maple {
  static int maple_socket;
  static SSL_CTX *ssl_context;

  auto exit_with[[noreturn]](const char *, bool) -> void;
  auto setup_environment(bool &, std::string &, size_t &) -> void;
  auto setup_ssl() -> void;
}

#endif // MAPLE_HH
