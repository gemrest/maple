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
#include <string>

namespace maple {
static int maple_socket;
static SSL_CTX *ssl_context;

auto prepare_exit_with(const char *, bool) -> int;
auto setup_environment(bool &, std::string &, size_t &) -> int;
auto setup_ssl() -> int;
} // namespace maple

#endif // MAPLE_HH
