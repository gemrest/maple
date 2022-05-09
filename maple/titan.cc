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

#include <map>
#include <fstream>
#include <vector>

#include "titan.hh"

namespace maple::titan {
  auto parameters_to_map(
    const std::vector<std::string> &parameters
  ) -> std::map<std::string, std::string> {
    std::map<std::string, std::string> parameters_map;

    for (auto parameter : parameters) {
      // Find the key in `parameter`
      size_t parameter_delimiter_position = parameter.find('=');
      std::string key = parameter.substr(
        0,
        parameter_delimiter_position
      );

      // Remove the key in `parameter`
      parameter.erase(0, parameter_delimiter_position + 1);

      // Add the key and value to `parameters_map`
      parameters_map[key] = parameter;
    }

    return parameters_map;
  }

  auto handle_client(
    std::stringstream &response,
    std::string path,
    const std::string &titan_token,
    size_t titan_max_size
  ) -> void {
    std::vector<std::string> parameters;
    // Find path in `path`
    size_t delimiter_position = path.find(';');
    std::string update_path = path.substr(0, delimiter_position);
    std::string body = path.substr(path.find('\n') + 1, path.length() - 1);

    path.erase(path.find('\n') - 1, path.length() - 1);
    // parameters.push_back(update_path);
    path.erase(0, delimiter_position + 1); // Remove path from `path`

    // Find mime parameter in `path`
    delimiter_position = path.find(';');

    parameters.push_back(path.substr(0, delimiter_position));
    path.erase(0, delimiter_position + 1); // Remove mime parameter from `path`

    // Find size parameter in `path`
    delimiter_position = path.find(';');

    parameters.push_back(path.substr(0, delimiter_position));

    // Find token parameter in `path`
    delimiter_position = path.find(';');

    // Since the token is optional, only get and assign the token
    // parameters value if it exists.
    if (delimiter_position != std::string::npos) {
      parameters.push_back(path.substr(
        delimiter_position + 1,
        path.length() - 1
      ));
    }

/// Check if a parameter exists within a `std::vector` of Titan
/// parameters.
    /* auto parameter_exists = [](
      const std::vector<std::string> &_parameters,
      const std::string &parameter
    ) -> bool {
      return std::any_of(
        _parameters.begin(),
        _parameters.end(),
        [&](const std::string &s) -> bool {
          return s.find(parameter) != std::string::npos;
        }
      );
    }; */

    std::map<std::string, std::string> parameters_map =
      maple::titan::parameters_to_map(parameters);

    // Make sure all tokens have been supplied
    for (;;) {
      if (parameters_map.find("mime") == parameters_map.end()) {
        response << "20 text/gemini\r\nThe serve (Maple) did not "
                    "receive a mime parameter!";
        break;
      }
      if (parameters_map.find("size") == parameters_map.end()) {
        response << "20 text/gemini\r\nThe serve (Maple) did not "
                    "receive a size parameter!";

        break;
      }
      if (!titan_token.empty()
        && parameters_map.find("token") == parameters_map.end())
      {
        response << "20 text/gemini\r\nThe serve (Maple) did not "
                    "receive a token parameter!";

        break;
      }

      try {
        size_t body_size = static_cast<size_t>(
          std::stoi(parameters_map["size"])
        );

        if (body_size > titan_max_size) {
          response << "20 text/gemini\r\nThe server (Maple) received a body "
            << "which is larger than the maximum allowed body size ("
            << titan_max_size << ").";

          break;
        }
      } catch (...) {
        response << "20 text/gemini\r\nThe server (Maple) could not interpret "
                    "the size parameter as an integer!";

        break;
      }

      if (update_path == "/") {
        update_path = "/index.gmi";
      }

      if (parameters_map["token"] == titan_token) {
        std::ofstream file(".maple/gmi" + update_path);

        file << body;

        response << "20 text/gemini\r\nSuccessfully wrote "
          << body.length() << " bytes to " << update_path << '!';
      } else {
        response << "20 text/gemini\r\nThe server (Maple) wrote to "
          << update_path;
      }

      break;
    }
  }
}
