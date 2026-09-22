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

#ifndef TITAN_HH
#define TITAN_HH

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace maple::titan {
struct Upload {
  std::string path;
  std::size_t size_bytes;
};

auto parse_upload(std::string_view header, std::string_view token,
                  std::size_t maximum_upload_bytes) -> std::optional<Upload>;
} // namespace maple::titan

#endif
