#ifndef MAPLE_DESCRIPTOR_HH
#define MAPLE_DESCRIPTOR_HH

#include <unistd.h>
#include <utility>

namespace maple {
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
} // namespace maple

#endif
