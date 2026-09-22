#ifndef MAPLE_CONTENT_HH
#define MAPLE_CONTENT_HH

#include <optional>
#include <string>
#include <string_view>

namespace maple::content {
auto read(std::string_view path) -> std::optional<std::string>;
auto write(std::string_view path, std::string_view body) -> bool;
} // namespace maple::content

#endif
