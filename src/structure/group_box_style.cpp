#include <arisa/components/group.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/structure/group_box_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

const composition::EnvironmentKey<GroupBoxStyle> GroupBoxStyle::key{
    "group_box_style", GroupBoxStyle::automatic()};

auto GroupBoxStyle::automatic() noexcept -> GroupBoxStyle {
  return GroupBoxStyle{Kind::automatic};
}

auto operator==(const GroupBoxStyle& left, const GroupBoxStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != GroupBoxStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto GroupBoxStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  std::vector<composition::Element> parts;
  if (!components::Group::is_empty(configuration.label)) {
    parts.push_back(std::move(configuration.label).font(theme::Font::headline));
  }
  parts.push_back(std::move(configuration.content));
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::Space{label_spacing}},
             std::move(parts))
      .padding(theme::Space{padding})
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .background(theme::ColourRole::quaternary_system_fill,
                  theme::Shape::rounded(theme::radius::card, true));
}

}
