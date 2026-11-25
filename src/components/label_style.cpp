#include <arisa/components/hstack.hpp>
#include <arisa/components/label_style.hpp>

#include <algorithm>
#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<LabelStyle> LabelStyle::key{
    "label_style", LabelStyle::automatic()};

auto LabelStyle::automatic() noexcept -> LabelStyle {
  return LabelStyle{Kind::automatic};
}

auto LabelStyle::title_and_icon() noexcept -> LabelStyle {
  return LabelStyle{Kind::title_and_icon};
}

auto LabelStyle::title_only() noexcept -> LabelStyle {
  return LabelStyle{Kind::title_only};
}

auto LabelStyle::icon_only() noexcept -> LabelStyle {
  return LabelStyle{Kind::icon_only};
}

auto operator==(const LabelStyle& left, const LabelStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != LabelStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto LabelStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::title_and_icon: {
      composition::Element icon = std::move(configuration.icon);
      if (configuration.reserved_icon_width.has_value()) {
        icon = std::move(icon).frame(
            {.width = std::max(*configuration.reserved_icon_width, 0.0F)});
      }
      return HStack(
          HStackOptions{
              .alignment = VerticalAlignment::center,
              .spacing = configuration.icon_to_title_spacing.has_value()
                             ? theme::Space{std::max(
                                   *configuration.icon_to_title_spacing, 0.0F)}
                             : theme::space::sm,
              .distribution = Distribution::start,
              .wrap = false},
          std::move(icon), std::move(configuration.title));
    }
    case Kind::title_only:
      return std::move(configuration.title);
    case Kind::icon_only:
      return std::move(configuration.icon);
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return std::move(configuration.title);
}

LabelStyle::LabelStyle(DefaultLabelStyle) noexcept : kind_(Kind::automatic) {}

auto DefaultLabelStyle::make_body(LabelStyle::Configuration configuration) const
    -> composition::Element {
  return LabelStyle{*this}.make_body(std::move(configuration));
}

LabelStyle::LabelStyle(TitleAndIconLabelStyle) noexcept
    : kind_(Kind::title_and_icon) {}

auto TitleAndIconLabelStyle::make_body(
    LabelStyle::Configuration configuration) const -> composition::Element {
  return LabelStyle{*this}.make_body(std::move(configuration));
}

LabelStyle::LabelStyle(TitleOnlyLabelStyle) noexcept
    : kind_(Kind::title_only) {}

auto TitleOnlyLabelStyle::make_body(
    LabelStyle::Configuration configuration) const -> composition::Element {
  return LabelStyle{*this}.make_body(std::move(configuration));
}

LabelStyle::LabelStyle(IconOnlyLabelStyle) noexcept : kind_(Kind::icon_only) {}

auto IconOnlyLabelStyle::make_body(
    LabelStyle::Configuration configuration) const -> composition::Element {
  return LabelStyle{*this}.make_body(std::move(configuration));
}

}
