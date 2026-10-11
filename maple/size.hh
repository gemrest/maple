#ifndef MAPLE_SIZE_HH
#define MAPLE_SIZE_HH

#include <charconv>
#include <cstddef>
#include <optional>
#include <string_view>

namespace maple {
inline auto parse_size(std::string_view text) -> std::optional<std::size_t> {
  if (text.empty()) {
    return std::nullopt;
  }

  std::size_t value = 0;
  const auto [parsed_end, error] =
      std::from_chars(text.begin(), text.end(), value);

  if (error != std::errc{} || parsed_end != text.end()) {
    return std::nullopt;
  }

  return value;
}
} // namespace maple

#endif
