#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include <string_view>

namespace arisa::components {

class Glyph {
 public:
  static auto symbol(std::string_view name, theme::ColourValue colour,
                     float size) -> composition::Element;

  static auto checkmark(theme::ColourValue colour, float size)
      -> composition::Element;

  static auto chevron_down(theme::ColourValue colour, float size)
      -> composition::Element;

  static auto chevron_right(theme::ColourValue colour, float size)
      -> composition::Element;

  static auto plus(theme::ColourValue colour, float size)
      -> composition::Element;

  static auto minus(theme::ColourValue colour, float size)
      -> composition::Element;

  static auto cross(theme::ColourValue colour, float size)
      -> composition::Element;
};

}
