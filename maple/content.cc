#include "content.hh"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using maple::Descriptor;

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
auto open_read(std::string_view path) -> Descriptor {
  auto destination = resolve(path);

  if (!destination) {
    return Descriptor(-1);
  }

  Descriptor file(openat(destination->directory.get(),
                         destination->name.c_str(),
                         O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
  struct stat status{};

  if (file.get() < 0 || fstat(file.get(), &status) < 0 ||
      !S_ISREG(status.st_mode)) {
    return Descriptor(-1);
  }

  return file;
}

auto write(std::string_view path, std::string_view body) -> bool {
  auto destination = resolve(path);

  if (!destination) {
    return false;
  }

  struct stat status{};
  const int existing =
      fstatat(destination->directory.get(), destination->name.c_str(), &status,
              AT_SYMLINK_NOFOLLOW);

  if ((existing == 0 && !S_ISREG(status.st_mode)) ||
      (existing < 0 && errno != ENOENT)) {
    return false;
  }

  static unsigned long sequence = 0;
  const std::string temporary_name = ".maple-upload-" +
                                     std::to_string(getpid()) + "-" +
                                     std::to_string(sequence++);
  Descriptor file(openat(destination->directory.get(), temporary_name.c_str(),
                         O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600));

  if (file.get() < 0) {
    return false;
  }

  bool complete = true;

  while (!body.empty()) {
    const ssize_t byte_count = ::write(file.get(), body.data(), body.size());

    if (byte_count < 0 && errno == EINTR) {
      continue;
    }

    if (byte_count <= 0) {
      complete = false;

      break;
    }

    body.remove_prefix(static_cast<std::size_t>(byte_count));
  }

  complete = complete && fsync(file.get()) == 0;

  if (complete) {
    complete =
        renameat(destination->directory.get(), temporary_name.c_str(),
                 destination->directory.get(), destination->name.c_str()) == 0;
  }

  if (!complete) {
    unlinkat(destination->directory.get(), temporary_name.c_str(), 0);
  }

  return complete;
}
} // namespace maple::content
