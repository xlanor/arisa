#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace arisa::components {

class GridItem {
 public:
  class Size {
   public:
    enum class Kind : std::uint8_t {
      fixed,
      flexible,
      adaptive,
    };

    static constexpr float default_minimum = 10.0F;

    static constexpr auto fixed(float length) noexcept -> Size {
      const float kept = length > 0.0F ? length : 0.0F;
      return Size{Kind::fixed, kept, kept};
    }

    static constexpr auto flexible(
        float minimum = default_minimum,
        float maximum = composition::infinity) noexcept -> Size {
      return Size{Kind::flexible, minimum, maximum};
    }

    static constexpr auto adaptive(
        float minimum, float maximum = composition::infinity) noexcept -> Size {
      return Size{Kind::adaptive, minimum, maximum};
    }

    constexpr auto kind() const noexcept -> Kind { return kind_; }

    constexpr auto minimum() const noexcept -> float { return minimum_; }

    constexpr auto maximum() const noexcept -> float { return maximum_; }

    friend constexpr auto operator==(const Size&, const Size&)
        -> bool = default;

   private:
    constexpr Size(Kind kind, float minimum, float maximum) noexcept
        : kind_(kind),
          minimum_(minimum > 0.0F ? minimum : 0.0F),
          maximum_(maximum > minimum_ ? maximum : minimum_) {}

    Kind kind_;
    float minimum_;
    float maximum_;
  };

  constexpr GridItem(Size size = Size::flexible(),
                     std::optional<theme::Space> spacing = std::nullopt,
                     std::optional<Alignment> alignment = std::nullopt) noexcept
      : size_(size), spacing_(spacing), alignment_(alignment) {}

  static auto repeated(std::size_t count, GridItem item = GridItem{})
      -> std::vector<GridItem>;

  constexpr auto size() const noexcept -> const Size& { return size_; }

  constexpr auto spacing() const noexcept
      -> const std::optional<theme::Space>& {
    return spacing_;
  }

  constexpr auto alignment() const noexcept -> const std::optional<Alignment>& {
    return alignment_;
  }

  friend constexpr auto operator==(const GridItem&, const GridItem&)
      -> bool = default;

 private:
  Size size_;
  std::optional<theme::Space> spacing_;
  std::optional<Alignment> alignment_;
};

}
