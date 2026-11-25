#include <arisa/components/glyph.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/theme/typography.hpp>

#include <string>

namespace arisa::components {

auto Glyph::symbol(std::string_view name, theme::ColourValue colour, float size)
    -> composition::Element {
  return ZStack(Image::system_name(std::string{name})
                    .font(theme::Font::system(size))
                    .foreground_style(colour))
      .frame({.width = size, .height = size})
      .allows_hit_testing(false);
}

auto Glyph::checkmark(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("check", colour, size);
}

auto Glyph::chevron_down(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("expand_more", colour, size);
}

auto Glyph::chevron_right(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("chevron_right", colour, size)
      .flips_for_right_to_left_layout_direction(true);
}

auto Glyph::plus(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("add", colour, size);
}

auto Glyph::minus(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("remove", colour, size);
}

auto Glyph::cross(theme::ColourValue colour, float size)
    -> composition::Element {
  return symbol("close", colour, size);
}

}
