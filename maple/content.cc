#include "content.hh"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace {
class Descriptor {
public:
  explicit Descriptor(int value) : value_(value) {}

  Descriptor(const Descriptor &) = delete;
  auto operator=(const Descriptor &) -> Descriptor & = delete;

  Descriptor(Descriptor &&other) noexcept
      : value_(std::exchange(other.value_, -1)) {}

  auto operator=(Descriptor &&other) noexcept -> Descriptor & {
    if (this != &other) {
      if (value_ >= 0) {
        close(value_);
      }

      value_ = std::exchange(other.value_, -1);
    }

    return *this;
  }

  ~Descriptor() {
    if (value_ >= 0) {
      close(value_);
    }
  }

  auto get() const -> int { return value_; }

private:
  int value_;
};

struct Destination {
  Descriptor directory;
  std::string name;
};

auto resolve(std::string_view path) -> std::optional<Destination> {
  if (path.empty() || path.front() != '/' ||
      path.find('\0') != std::string_view::npos) {
    return std::nullopt;
  }

  std::string relative_path(path.substr(1));

  if (relative_path.empty() || relative_path.back() == '/') {
    relative_path += "index.gmi";
  }

  Descriptor directory(open(".maple/gmi", O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
  std::size_t component_start_bytes = 0;

  if (directory.get() < 0) {
    return std::nullopt;
  }

  for (;;) {
    const std::size_t separator_offset_bytes =
        relative_path.find('/', component_start_bytes);
    const std::string component = relative_path.substr(
        component_start_bytes, separator_offset_bytes - component_start_bytes);

    if (component.empty() || component == "." || component == "..") {
      return std::nullopt;
    }

    if (separator_offset_bytes == std::string::npos) {
      const std::size_t extension_start_bytes = component.find_last_of('.');
      std::string extension = extension_start_bytes == std::string::npos
                                  ? ""
                                  : component.substr(extension_start_bytes);

      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });

      if (extension != ".gmi") {
        return std::nullopt;
      }

      return Destination{std::move(directory), component};
    }

    const int child = openat(directory.get(), component.c_str(),
                             O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

    if (child < 0) {
      return std::nullopt;
    }

    directory = Descriptor(child);
    component_start_bytes = separator_offset_bytes + 1;
  }
}
} // namespace

namespace maple::content {
auto read(std::string_view path) -> std::optional<std::string> {
  auto destination = resolve(path);

  if (!destination) {
    return std::nullopt;
  }

  Descriptor file(openat(destination->directory.get(),
                         destination->name.c_str(),
                         O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
  struct stat status{};

  if (file.get() < 0 || fstat(file.get(), &status) < 0 ||
      !S_ISREG(status.st_mode)) {
    return std::nullopt;
  }

  constexpr std::size_t buffer_capacity_bytes = 8192;
  std::string body;
  std::array<char, buffer_capacity_bytes> buffer{};

  for (;;) {
    const ssize_t byte_count = ::read(file.get(), buffer.data(), buffer.size());

    if (byte_count < 0) {
      if (errno == EINTR) {
        continue;
      }

      return std::nullopt;
    }

    if (byte_count == 0) {
      return body;
    }

    body.append(buffer.data(), static_cast<std::size_t>(byte_count));
  }
}

auto write(std::string_view path, std::string_view body) -> bool {
  auto destination = resolve(path);

  if (!destination) {
    return false;
  }

  Descriptor file(openat(destination->directory.get(),
                         destination->name.c_str(),
                         O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0600));
  struct stat status{};

  if (file.get() < 0 || fstat(file.get(), &status) < 0 ||
      !S_ISREG(status.st_mode) || ftruncate(file.get(), 0) < 0) {
    return false;
  }

  while (!body.empty()) {
    const ssize_t byte_count = ::write(file.get(), body.data(), body.size());

    if (byte_count < 0 && errno == EINTR) {
      continue;
    }

    if (byte_count <= 0) {
      return false;
    }

    body.remove_prefix(static_cast<std::size_t>(byte_count));
  }

  return true;
}
} // namespace maple::content
