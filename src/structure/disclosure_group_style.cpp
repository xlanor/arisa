#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/disclosure_group_style.hpp>
#include <arisa/theme/colour_value.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

const composition::EnvironmentKey<DisclosureGroupStyle>
    DisclosureGroupStyle::key{"disclosure_group_style",
                              DisclosureGroupStyle::automatic()};

auto DisclosureGroupStyle::automatic() noexcept -> DisclosureGroupStyle {
  return DisclosureGroupStyle{Kind::automatic};
}

auto operator==(const DisclosureGroupStyle& left,
                const DisclosureGroupStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != DisclosureGroupStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto DisclosureGroupStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  return disclosed(std::move(configuration));
}

auto DisclosureGroupStyle::disclosed(Configuration configuration)
    -> composition::Element {
  const composition::Binding<bool> open = configuration.is_expanded;
  const bool shown = open.get();
  composition::Element header =
      components::Button(
          [open] {
            motion::with_animation(motion::Animation::snappy(0.3),
                                   [&open] { open.set(!open.get()); });
          },
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = theme::space::sm},
              std::move(configuration.label), components::Spacer(),
              components::Glyph::chevron_right(theme::ColourRole::accent,
                                               chevron_extent)
                  .rotation_effect(
                      motion::Angle::degrees(shown ? expanded_degrees : 0.0))
                  .animation(motion::Animation::snappy(0.3), shown)
                  .id("arisa.disclosure.chevron"))
              .frame({.max_width = composition::infinity}))
          .button_style(components::ButtonStyle::plain())
          .frame({.max_width = composition::infinity})
          .id("arisa.disclosure.label");
  std::vector<composition::Element> column;
  column.push_back(std::move(header));
  if (shown && configuration.content) {
    column.push_back(
        components::VStack(
            components::VStackOptions{
                .alignment = components::HorizontalAlignment::leading,
                .spacing = theme::space::none},
            components::Group::members(configuration.content()))
            .padding(theme::Insets{.top = theme::space::xs,
                                   .leading = indent,
                                   .bottom = theme::space::none,
                                   .trailing = theme::space::none})
            .frame({.max_width = composition::infinity,
                    .alignment = composition::Alignment::leading})
            .transition(motion::Transition::opacity())
            .id("arisa.disclosure.content"));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::none},
             std::move(column))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

}
