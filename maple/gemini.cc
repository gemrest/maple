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
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gemini.hh"

namespace maple::gemini {
auto handle_client(std::vector<std::string> gemini_files, std::string path,
                   std::stringstream &response) -> void {
  // Check if the route is a file being served
  if (std::find(gemini_files.begin(), gemini_files.end(),
                ".maple/gmi" + path) != gemini_files.end()) {
    // If the route is a file being served; get the file contents
    response << "20 text/gemini\r\n"
             << std::ifstream(".maple/gmi" + path).rdbuf();
  } else {
    if (path.empty() || path.at(path.length() - 1) == '/') {
      response << "20 text/gemini\r\n"
               << std::ifstream(".maple/gmi" + path + "index.gmi").rdbuf();
    } else {
      response
          << "51 The server (Maple) could not find the specified file.\r\n";
    }
  }

  std::cout << "requested " << path << '\n';
}
} // namespace maple::gemini
