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

#include "titan.hh"
#include "size.hh"

namespace maple::titan {
auto parse_upload(std::string_view header, std::string_view token,
                  std::size_t maximum_upload_bytes) -> std::optional<Upload> {
  const std::size_t separator_offset_bytes = header.find(';');

  if (separator_offset_bytes == std::string_view::npos ||
      separator_offset_bytes == 0) {
    return std::nullopt;
  }

  const std::string path(header.substr(0, separator_offset_bytes));
  std::optional<std::size_t> size_bytes;
  std::optional<std::string_view> mime;
  std::optional<std::string_view> supplied_token;

  header.remove_prefix(separator_offset_bytes + 1);

  while (!header.empty()) {
    const std::size_t next_parameter_offset_bytes = header.find(';');
    const std::string_view parameter =
        header.substr(0, next_parameter_offset_bytes);
    const std::size_t value_separator_offset_bytes = parameter.find('=');

    if (value_separator_offset_bytes == std::string_view::npos) {
      return std::nullopt;
    }

    const std::string_view key =
        parameter.substr(0, value_separator_offset_bytes);
    const std::string_view value =
        parameter.substr(value_separator_offset_bytes + 1);

    if (key == "size" && !size_bytes) {
      size_bytes = parse_size(value);

      if (!size_bytes || *size_bytes > maximum_upload_bytes) {
        return std::nullopt;
      }
    } else if (key == "mime" && !mime) {
      mime = value;
    } else if (key == "token" && !supplied_token) {
      supplied_token = value;
    } else {
      return std::nullopt;
    }

    if (next_parameter_offset_bytes == std::string_view::npos) {
      break;
    }

    header.remove_prefix(next_parameter_offset_bytes + 1);

    if (header.empty()) {
      return std::nullopt;
    }
  }

  if (!size_bytes || !mime || *mime != "text/gemini" ||
      supplied_token.value_or("") != token) {
    return std::nullopt;
  }

  return Upload{path, *size_bytes};
}
} // namespace maple::titan
