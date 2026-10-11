#ifndef MAPLE_URI_HH
#define MAPLE_URI_HH

#include <optional>
#include <string>
#include <string_view>

namespace maple::uri {
struct Request {
  bool titan;
  std::string_view path;
};

// Keep the request URL alive while using the returned path.
auto parse_request(std::string_view url) -> std::optional<Request>;
auto decode_segment(std::string_view encoded) -> std::optional<std::string>;
} // namespace maple::uri

#endif
