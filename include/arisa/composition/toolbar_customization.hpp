#pragma once

#include <arisa/composition/visibility.hpp>

#include <cstdint>
#include <flat_map>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::composition {

enum class ToolbarCustomizationBehavior : std::uint8_t {
  default_behavior,
  disabled,
  reorderable,
};

class ToolbarCustomizationOptions {
 public:
  static const ToolbarCustomizationOptions always_available;

  constexpr ToolbarCustomizationOptions() noexcept = default;

  constexpr auto contains(ToolbarCustomizationOptions other) const noexcept
      -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0U;
  }

  friend constexpr auto operator|(ToolbarCustomizationOptions left,
                                  ToolbarCustomizationOptions right) noexcept
      -> ToolbarCustomizationOptions {
    return ToolbarCustomizationOptions{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(ToolbarCustomizationOptions,
                                   ToolbarCustomizationOptions)
      -> bool = default;

 private:
  constexpr explicit ToolbarCustomizationOptions(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr ToolbarCustomizationOptions
    ToolbarCustomizationOptions::always_available{1U};

class ToolbarCustomization {
 public:
  ToolbarCustomization() = default;

  auto visibility(std::string_view id) const -> Visibility;

  void set_visibility(std::string_view id, Visibility visibility);

  void reset();

  auto empty() const noexcept -> bool { return visibility_.empty(); }

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text)
      -> std::optional<ToolbarCustomization>;

  friend auto operator==(const ToolbarCustomization&,
                         const ToolbarCustomization&) -> bool = default;

 private:
  static constexpr char hidden_mark = 'h';
  static constexpr char visible_mark = 'v';
  static constexpr char separator = '\n';

  std::flat_map<std::string, Visibility, std::less<>> visibility_;
};

}
