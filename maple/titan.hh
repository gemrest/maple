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

#include <map>
#include <sstream>
#include <vector>

namespace maple {
namespace titan {
/// Convert a `std::vector` of Titan parameters into a key/ value `std::map`
auto parameters_to_map(const std::vector<std::string> &)
    -> std::map<std::string, std::string>;

auto handle_client(std::stringstream &, std::string, const std::string &,
                   std::size_t) -> void;
} // namespace titan
} // namespace maple

#endif // TITAN_HH
