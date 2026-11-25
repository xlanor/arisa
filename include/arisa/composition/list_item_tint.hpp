#pragma once

#include <arisa/theme/colour.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class ListItemTint {
 public:
  enum class Kind : std::uint8_t {
    fixed,
    preferred,
    monochrome,
  };

  static auto fixed(theme::Colour colour) noexcept -> ListItemTint {
    return ListItemTint{Kind::fixed, colour};
  }

  static auto preferred(theme::Colour colour) noexcept -> ListItemTint {
    return ListItemTint{Kind::preferred, colour};
  }

  static auto monochrome() noexcept -> ListItemTint {
    return ListItemTint{Kind::monochrome, std::nullopt};
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto colour() const noexcept -> std::optional<theme::Colour> {
    return colour_;
  }

  friend auto operator==(const ListItemTint&, const ListItemTint&)
      -> bool = default;

 private:
  ListItemTint(Kind kind, std::optional<theme::Colour> colour) noexcept
      : kind_(kind), colour_(colour) {}

  Kind kind_;
  std::optional<theme::Colour> colour_;
};

}
