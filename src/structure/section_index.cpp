#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/structure/section_index.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>

namespace arisa::structure {

auto SectionIndex::key_of(std::size_t section) -> composition::ElementKey {
  return composition::ElementKey{std::format("arisa.list.section.{}", section)};
}

void SectionIndex::jump(std::size_t index) const {
  if (index < entries_.size()) {
    proxy_.scroll_to(entries_[index].key, motion::UnitPoint::top);
  }
}

auto SectionIndex::body(composition::Context&) const -> composition::Element {
  std::vector<composition::Element> letters;
  letters.reserve(entries_.size());
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    letters.push_back(components::Button(
                          [index, this_copy = *this] { this_copy.jump(index); },
                          components::Text(entries_[index].label)
                              .font(theme::Font::caption2)
                              .foreground_style(theme::ColourRole::accent)
                              .frame({.width = width, .height = row_height}))
                          .button_style(components::ButtonStyle::plain())
                          .id(static_cast<std::int64_t>(index)));
  }
  const auto count = entries_.size();
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::none},
             std::move(letters))
      .frame({.width = width})
      .simultaneous_gesture(
          composition::DragGesture(0.0F, composition::CoordinateSpace::local)
              .on_changed([index = *this, count](
                              const composition::DragGesture::Value& value) {
                if (count == 0) {
                  return;
                }
                const long at =
                    std::lround(std::floor(value.location.y / row_height));
                index.jump(static_cast<std::size_t>(
                    std::clamp(at, 0L, static_cast<long>(count) - 1L)));
              }))
      .focus_section()
      .id("arisa.list.index");
}

}
