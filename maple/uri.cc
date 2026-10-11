#include "uri.hh"
#include "size.hh"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstdint>
#include <limits>

namespace {
constexpr std::size_t encoded_byte_characters = 3;
constexpr std::string_view sub_delimiters = "!$&'()*+,;=";

auto hex_value(char character) -> int {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }

  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }

  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }

  return -1;
}

auto is_unreserved(char character) -> bool {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') ||
         std::string_view("-._~").find(character) != std::string_view::npos;
}

auto valid_component(std::string_view text, std::string_view delimiters)
    -> bool {
  while (!text.empty()) {
    if (text.front() == '%') {
      if (text.size() < encoded_byte_characters || hex_value(text[1]) < 0 ||
          hex_value(text[2]) < 0) {
        return false;
      }

      text.remove_prefix(encoded_byte_characters);

      continue;
    }

    if (!is_unreserved(text.front()) &&
        sub_delimiters.find(text.front()) == std::string_view::npos &&
        delimiters.find(text.front()) == std::string_view::npos) {
      return false;
    }

    text.remove_prefix(1);
  }

  return true;
}

auto valid_authority(std::string_view authority) -> bool {
  if (authority.empty()) {
    return false;
  }

  const std::string_view host = authority.substr(0, authority.find(':'));
  std::string_view suffix = authority.substr(host.size());

  if (authority.front() == '[') {
    const std::size_t closing_bracket_offset_bytes = authority.find(']');

    if (closing_bracket_offset_bytes == std::string_view::npos) {
      return false;
    }

    const std::string literal(
        authority.substr(1, closing_bracket_offset_bytes - 1));
    in6_addr address{};

    if (inet_pton(AF_INET6, literal.c_str(), &address) != 1) {
      return false;
    }

    suffix = authority.substr(closing_bracket_offset_bytes + 1);
  } else if (host.empty() || !valid_component(host, "")) {
    return false;
  }

  if (suffix.empty()) {
    return true;
  }

  if (suffix.front() != ':') {
    return false;
  }

  suffix.remove_prefix(1);

  if (suffix.empty()) {
    return true;
  }

  const auto port = maple::parse_size(suffix);

  return port && *port > 0 &&
         *port <= std::numeric_limits<std::uint16_t>::max();
}

auto same_scheme(std::string_view scheme, std::string_view expected) -> bool {
  return std::equal(scheme.begin(), scheme.end(), expected.begin(),
                    expected.end(),
                    [](unsigned char character, char lowercase) {
                      return std::tolower(character) == lowercase;
                    });
}
} // namespace

namespace maple::uri {
auto parse_request(std::string_view url) -> std::optional<Request> {
  constexpr std::string_view scheme_separator = "://";
  const std::size_t scheme_end_bytes = url.find(scheme_separator);
  const std::string_view scheme = url.substr(0, scheme_end_bytes);
  const bool titan = same_scheme(scheme, "titan");

  if (scheme_end_bytes == std::string_view::npos ||
      (!titan && !same_scheme(scheme, "gemini")) ||
      url.find('#') != std::string_view::npos) {
    return std::nullopt;
  }

  url.remove_prefix(scheme_end_bytes + scheme_separator.size());

  const std::string_view authority = url.substr(0, url.find_first_of("/?#"));

  if (!valid_authority(authority)) {
    return std::nullopt;
  }

  url.remove_prefix(authority.size());

  const std::size_t query_start_bytes = url.find('?');
  const std::string_view path = url.substr(0, query_start_bytes);
  const std::string_view query = query_start_bytes == std::string_view::npos
                                     ? ""
                                     : url.substr(query_start_bytes + 1);

  if (!valid_component(path, ":@/") || !valid_component(query, ":@/?")) {
    return std::nullopt;
  }

  return Request{titan, path.empty() ? "/" : path};
}

auto decode_segment(std::string_view encoded) -> std::optional<std::string> {
  std::string decoded;

  decoded.reserve(encoded.size());

  while (!encoded.empty()) {
    char character = encoded.front();

    if (character == '%') {
      if (encoded.size() < encoded_byte_characters) {
        return std::nullopt;
      }

      const int high = hex_value(encoded[1]);
      const int low = hex_value(encoded[2]);

      if (high < 0 || low < 0) {
        return std::nullopt;
      }

      character = static_cast<char>(high * 16 + low);

      encoded.remove_prefix(encoded_byte_characters);
    } else {
      encoded.remove_prefix(1);
    }

    if (character == '\0' || character == '/' || character == '\\') {
      return std::nullopt;
    }

    decoded += character;
  }

  return decoded;
}
} // namespace maple::uri
