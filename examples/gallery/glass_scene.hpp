#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

namespace arisa::examples {

class GlassScene final : public composition::Component {
 public:
  static constexpr float width = 3200.0F;
  static constexpr float height = 280.0F;

  struct Box {
    composition::Rect rect;
    float radius;
    std::uint32_t colour;
  };

  struct Oval {
    composition::Rect rect;
    std::uint32_t colour;
  };

  struct Bars {
    composition::Rect rect;
    float period;
    std::uint32_t first;
    std::uint32_t second;
  };

  struct Checks {
    composition::Rect rect;
    float cell;
    std::uint32_t first;
    std::uint32_t second;
  };

  struct Stroke {
    composition::Point from;
    composition::Point to;
    float width;
    std::uint32_t colour;
  };

  using Item = std::variant<Box, Oval, Bars, Checks, Stroke>;

  static constexpr float period = 330.0F;

  static constexpr std::array<Item, 2> spans{
      Box{{0.0F, 140.0F, width, 140.0F}, 0.0F, 0x3F8F50},
      Bars{{0.0F, 32.0F, width, 32.0F}, 16.0F, 0xF4F4F4, 0x1C1C1E},
  };

  static constexpr std::array<Item, 9> motifs{
      Oval{{-40.0F, 106.0F, 420.0F, 190.0F}, 0x79C27D},
      Checks{{0.0F, 12.0F, 120.0F, 72.0F}, 12.0F, 0xFFFFFF, 0x202020},
      Stroke{{146.0F, 16.0F}, {236.0F, 80.0F}, 5.0F, 0xD93636},
      Stroke{{146.0F, 80.0F}, {236.0F, 16.0F}, 5.0F, 0xD93636},
      Stroke{{191.0F, 8.0F}, {191.0F, 88.0F}, 5.0F, 0xD93636},
      Oval{{262.0F, 19.0F, 58.0F, 58.0F}, 0xFFD24A},
      Box{{150.0F, 94.0F, 160.0F, 3.0F}, 1.5F, 0x1C1C1E},
      Box{{150.0F, 102.0F, 124.0F, 3.0F}, 1.5F, 0x1C1C1E},
      Box{{150.0F, 110.0F, 176.0F, 3.0F}, 1.5F, 0x1C1C1E},
  };

  auto operator==(const GlassScene&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto drawn(const Item& item, float spacing, int tiles)
      -> composition::Element;

  static auto filled(graphics::Path path, std::uint32_t colour)
      -> composition::Element;

  static auto patterned(const Item& item, float spacing, int tiles)
      -> std::array<graphics::Path, 2>;

  static auto layered() -> std::vector<composition::Element>;
};

}
