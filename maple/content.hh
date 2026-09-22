#ifndef MAPLE_CONTENT_HH
#define MAPLE_CONTENT_HH

#include "descriptor.hh"
#include <optional>
#include <string>
#include <string_view>

namespace maple::content {
auto open_read(std::string_view path) -> Descriptor;
auto write(std::string_view path, std::string_view body) -> bool;
} // namespace maple::content

#endif
