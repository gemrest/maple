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

  const char *text_end = text.data() + text.size();
  std::size_t value = 0;
  const auto [parsed_end, error] =
      std::from_chars(text.data(), text_end, value);

  if (error != std::errc{} || parsed_end != text_end) {
    return std::nullopt;
  }

  return value;
}
} // namespace maple

#endif
