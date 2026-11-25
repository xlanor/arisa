#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

class AccessibilityTechnologies {
 public:
  static const AccessibilityTechnologies voice_over;
  static const AccessibilityTechnologies switch_control;

  constexpr AccessibilityTechnologies() noexcept = default;

  constexpr auto contains(AccessibilityTechnologies member) const noexcept
      -> bool {
    return member.bits_ != 0U && (bits_ & member.bits_) == member.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto union_with(AccessibilityTechnologies other) const noexcept
      -> AccessibilityTechnologies {
    return AccessibilityTechnologies{
        static_cast<std::uint8_t>(bits_ | other.bits_)};
  }

  constexpr auto intersection(AccessibilityTechnologies other) const noexcept
      -> AccessibilityTechnologies {
    return AccessibilityTechnologies{
        static_cast<std::uint8_t>(bits_ & other.bits_)};
  }

  constexpr auto symmetric_difference(AccessibilityTechnologies other)
      const noexcept -> AccessibilityTechnologies {
    return AccessibilityTechnologies{
        static_cast<std::uint8_t>(bits_ ^ other.bits_)};
  }

  constexpr void form_union(AccessibilityTechnologies other) noexcept {
    *this = union_with(other);
  }

  constexpr void form_intersection(AccessibilityTechnologies other) noexcept {
    *this = intersection(other);
  }

  constexpr void form_symmetric_difference(
      AccessibilityTechnologies other) noexcept {
    *this = symmetric_difference(other);
  }

  constexpr auto insert(AccessibilityTechnologies member) noexcept
      -> std::pair<bool, AccessibilityTechnologies> {
    if (contains(member)) {
      return {false, member};
    }
    form_union(member);
    return {true, member};
  }

  constexpr auto remove(AccessibilityTechnologies member) noexcept
      -> std::optional<AccessibilityTechnologies> {
    const AccessibilityTechnologies held = intersection(member);
    bits_ = static_cast<std::uint8_t>(bits_ & ~member.bits_);
    return held.empty() ? std::nullopt : std::optional{held};
  }

  constexpr auto update(AccessibilityTechnologies with) noexcept
      -> std::optional<AccessibilityTechnologies> {
    const AccessibilityTechnologies held = intersection(with);
    form_union(with);
    return held.empty() ? std::nullopt : std::optional{held};
  }

  friend constexpr auto operator|(AccessibilityTechnologies left,
                                  AccessibilityTechnologies right) noexcept
      -> AccessibilityTechnologies {
    return left.union_with(right);
  }

  friend constexpr auto operator==(AccessibilityTechnologies,
                                   AccessibilityTechnologies) -> bool = default;

 private:
  constexpr explicit AccessibilityTechnologies(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr AccessibilityTechnologies
    AccessibilityTechnologies::voice_over{1U};
inline constexpr AccessibilityTechnologies
    AccessibilityTechnologies::switch_control{2U};

}
