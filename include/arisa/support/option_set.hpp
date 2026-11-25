#pragma once

#include <cstdint>

namespace arisa::support {

template <class Self>
class OptionSet {
 public:
  constexpr OptionSet() noexcept = default;

  constexpr auto contains(Self other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0U;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(Self left, Self right) noexcept -> Self {
    Self joined = left;
    joined.bits_ = static_cast<std::uint8_t>(left.bits_ | right.bits_);
    return joined;
  }

  friend constexpr auto operator==(const OptionSet&, const OptionSet&)
      -> bool = default;

 protected:
  constexpr explicit OptionSet(std::uint8_t bits) noexcept : bits_(bits) {}

 private:
  std::uint8_t bits_{0};
};

}
