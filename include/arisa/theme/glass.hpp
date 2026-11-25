#pragma once

#include <arisa/theme/colour_value.hpp>

#include <cstdint>
#include <optional>

namespace arisa::theme {

class Glass {
 public:
  enum class Variant : std::uint8_t {
    regular,
    clear,
    identity,
  };

  static constexpr auto regular() noexcept -> Glass {
    return Glass{Variant::regular};
  }

  static constexpr auto clear() noexcept -> Glass {
    return Glass{Variant::clear};
  }

  static constexpr auto identity() noexcept -> Glass {
    return Glass{Variant::identity};
  }

  constexpr auto tint(std::optional<ColourValue> colour) const noexcept
      -> Glass {
    Glass tinted = *this;
    tinted.tint_ = colour;
    return tinted;
  }

  constexpr auto interactive(bool is_enabled = true) const noexcept -> Glass {
    Glass changed = *this;
    changed.interactive_ = is_enabled;
    return changed;
  }

  constexpr auto variant() const noexcept -> Variant { return variant_; }

  constexpr auto tint_colour() const noexcept
      -> const std::optional<ColourValue>& {
    return tint_;
  }

  constexpr auto is_interactive() const noexcept -> bool {
    return interactive_;
  }

  friend constexpr auto operator==(const Glass&, const Glass&)
      -> bool = default;

 private:
  constexpr explicit Glass(Variant variant) noexcept : variant_(variant) {}

  Variant variant_;
  std::optional<ColourValue> tint_;
  bool interactive_{false};
};

enum class GlassSlot : std::uint8_t {
  sidebar,
  toolbar,
  sheet,
};

struct GlassDefaults {
  std::optional<Glass> sidebar;
  std::optional<Glass> toolbar;
  std::optional<Glass> sheet;
  Glass control{Glass::regular()};
  Glass panel{Glass::regular()};

  constexpr auto slot(GlassSlot which) const noexcept
      -> const std::optional<Glass>& {
    switch (which) {
      case GlassSlot::toolbar:
        return toolbar;
      case GlassSlot::sheet:
        return sheet;
      case GlassSlot::sidebar:
        break;
    }
    return sidebar;
  }

  friend constexpr auto operator==(const GlassDefaults&, const GlassDefaults&)
      -> bool = default;
};

}
