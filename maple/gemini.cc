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

#include "gemini.hh"
#include "content.hh"
#include <array>
#include <cerrno>

namespace maple::gemini {
auto handle_client(Connection &connection, std::string_view path) -> bool {
  const auto file = content::open_read(path);

  if (file.get() < 0) {
    return connection.write("51 File not found.\r\n");
  }

  if (!connection.write("20 text/gemini\r\n")) {
    return false;
  }

  constexpr std::size_t buffer_capacity_bytes = 8192;
  std::array<char, buffer_capacity_bytes> buffer{};

  for (;;) {
    const ssize_t byte_count = ::read(file.get(), buffer.data(), buffer.size());

    if (byte_count < 0 && errno == EINTR) {
      continue;
    }

    if (byte_count < 0) {
      return false;
    }

    if (byte_count == 0) {
      return true;
    }

    if (!connection.write(std::string_view(
            buffer.data(), static_cast<std::size_t>(byte_count)))) {
      return false;
    }
  }
}
} // namespace maple::gemini
