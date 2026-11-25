#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/provider/input/provider.hpp>

#include <interaction/input_router.hpp>

#include <cstdint>
#include <limits>
#include <optional>

namespace arisa::runtime {

class PointerCursor {
 public:
  static constexpr std::uint64_t overlay =
      std::numeric_limits<std::uint64_t>::max() - 1;
  static constexpr float canvas = 24.0F;
  static constexpr float dot = 18.0F;
  static constexpr float outline = 1.5F;
  static constexpr float dot_opacity = 0.45F;

  void present(composition::Tree& tree,
               const std::optional<interaction::InputRouter::Pointer>& pointer,
               provider::InputProvider& input);

  static auto shape_of(const composition::PointerStyle& style) noexcept
      -> provider::PointerShape;

  static auto hot_spot(const composition::PointerStyle& style) noexcept
      -> composition::Point;

  static auto art(const composition::PointerStyle& style)
      -> composition::Element;

 private:
  static auto glyph(graphics::Path path) -> composition::Element;

  static auto arrow(bool both) -> graphics::Path;

  static auto hand(bool closed) -> graphics::Path;

  static auto magnifier(bool plus) -> graphics::Path;

  static auto resize(provider::PointerShape shape) -> composition::Element;

  void hide(composition::Tree& tree);

  std::optional<interaction::InputRouter::Pointer> shown_;
  std::optional<provider::PointerAppearance> presented_;
};

}
