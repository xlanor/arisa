#include <arisa/components/glyph.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

const composition::EnvironmentKey<CellStyle> CellStyle::key{
    "cell_style", CellStyle::automatic()};

CellStyle::CellStyle(DefaultCellStyle) noexcept : kind_(Kind::automatic) {}

auto CellStyle::automatic() noexcept -> CellStyle {
  return CellStyle{Kind::automatic};
}

auto operator==(const CellStyle& left, const CellStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != CellStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto CellStyle::insets() noexcept -> theme::Insets {
  return insets(ListStyle::inset_grouped());
}

auto CellStyle::insets(const ListStyle& list) noexcept -> theme::Insets {
  const float height = default_row_height(list);
  const float vertical = (height - theme::metrics::button.height) * 0.5F;
  switch (list.kind()) {
    case ListStyle::Kind::plain:
    case ListStyle::Kind::inset:
    case ListStyle::Kind::bordered:
      return theme::Insets::symmetric(theme::metric::list_plain_row_inset,
                                      vertical);
    case ListStyle::Kind::sidebar:
      return theme::Insets::symmetric(theme::metric::sidebar_selection_inset,
                                      vertical);
    case ListStyle::Kind::inset_grouped:
    case ListStyle::Kind::grouped:
      break;
  }
  return theme::Insets::symmetric(theme::metric::list_grouped_row_inset,
                                  vertical);
}

auto CellStyle::row_height(const ListStyle& list) noexcept
    -> theme::ScaledMetric {
  switch (list.kind()) {
    case ListStyle::Kind::plain:
    case ListStyle::Kind::inset:
    case ListStyle::Kind::bordered:
      return theme::metric::list_plain_row_height;
    case ListStyle::Kind::sidebar:
      return theme::metric::sidebar_row_height;
    case ListStyle::Kind::inset_grouped:
    case ListStyle::Kind::grouped:
      break;
  }
  return theme::metric::list_grouped_row_height;
}

auto CellStyle::default_row_height(const ListStyle& list) noexcept -> float {
  switch (list.kind()) {
    case ListStyle::Kind::plain:
    case ListStyle::Kind::inset:
    case ListStyle::Kind::bordered:
      return theme::metrics::list.plain_row_height;
    case ListStyle::Kind::sidebar:
      return theme::metrics::list.sidebar_row_height;
    case ListStyle::Kind::inset_grouped:
    case ListStyle::Kind::grouped:
      break;
  }
  return theme::metrics::list.grouped_row_height;
}

auto CellStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  switch (configuration.list_style.kind()) {
    case ListStyle::Kind::inset_grouped:
    case ListStyle::Kind::grouped:
      return grouped(std::move(configuration));
    case ListStyle::Kind::plain:
    case ListStyle::Kind::inset:
    case ListStyle::Kind::bordered:
      return plain(std::move(configuration));
    case ListStyle::Kind::sidebar:
      return sidebar(std::move(configuration));
  }
  return grouped(std::move(configuration));
}

auto CellStyle::row(Configuration& configuration) -> composition::Element {
  std::vector<composition::Element> items;
  items.push_back(std::move(configuration.content)
                      .frame({.max_width = composition::infinity,
                              .alignment = composition::Alignment::leading}));
  if (configuration.shows_disclosure) {
    items.push_back(components::Glyph::chevron_right(
        theme::ColourRole::text_secondary, chevron_extent));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::sm},
             std::move(items))
      .padding(configuration.insets.has_value()
                   ? *configuration.insets
                   : (configuration.fills ? theme::Insets{}
                                          : insets(configuration.list_style)))
      .frame({.max_width = composition::infinity,
              .min_height =
                  configuration.minimum_height.has_value()
                      ? composition::Length{*configuration.minimum_height}
                      : composition::Length{
                            row_height(configuration.list_style)}});
}

auto CellStyle::card(bool first, bool last, float radius)
    -> composition::Element {
  const theme::Shape rounded = theme::Shape::rounded(radius, true);
  std::vector<composition::Element> layers;
  if (first || last) {
    layers.push_back(
        components::ZStack()
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .background(theme::ColourRole::surface_raised, rounded));
  }
  if (!first || !last) {
    composition::Element square =
        components::ZStack()
            .frame({.height = composition::Length::percent(
                        first || last ? 0.5F : 1.0F),
                    .max_width = composition::infinity})
            .background(theme::ColourRole::surface_raised);
    layers.push_back(std::move(square));
  }
  const composition::Alignment anchor =
      first ? composition::Alignment::bottom : composition::Alignment::top;
  return components::ZStack(components::ZStackOptions{.alignment = anchor},
                            std::move(layers))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .allows_hit_testing(false);
}

auto CellStyle::fill(Configuration& configuration, bool first, bool last,
                     float radius) -> composition::Element {
  if (!configuration.background.has_value()) {
    return card(first, last, radius);
  }
  composition::Element view = std::move(*configuration.background)
                                  .frame({.max_width = composition::infinity,
                                          .max_height = composition::infinity})
                                  .allows_hit_testing(false);
  if (first && last && radius > 0.0F) {
    view = std::move(view).clip_shape(theme::Shape::rounded(radius, true));
  }
  return view;
}

auto CellStyle::separated(composition::Element body,
                          const Configuration& configuration, bool hidden)
    -> composition::Element {
  return std::move(body).overlay(
      composition::Alignment::top,
      separator(theme::metric::list_grouped_row_inset,
                configuration.separator_tint)
          .opacity(hidden || !configuration.shows_separator ? 0.0F : 1.0F)
          .id("arisa.list.row-separator"));
}

auto CellStyle::separator(theme::Space inset,
                          std::optional<theme::ColourValue> tint)
    -> composition::Element {
  return components::HStack(
             components::ZStack()
                 .frame({.height = 0.5F, .max_width = composition::infinity})
                 .background(tint.value_or(theme::ColourRole::separator)))
      .padding(theme::Insets{.leading = inset})
      .frame({.max_width = composition::infinity})
      .allows_hit_testing(false);
}

auto CellStyle::section_separator(std::optional<theme::ColourValue> tint)
    -> composition::Element {
  return separator(theme::metric::list_grouped_row_inset, tint)
      .id("arisa.list.section-separator");
}

auto CellStyle::lift(composition::Element element, bool lifted)
    -> composition::Element {
  if (lifted) {
    element = std::move(element)
                  .scale_effect(lifted_scale)
                  .shadow(theme::Elevation::high);
  }
  return std::move(element).animation(motion::Animation::snappy(0.25), lifted);
}

auto CellStyle::grouped(Configuration configuration) -> composition::Element {
  const bool first = configuration.is_first;
  const bool last = configuration.is_last;
  const bool lifted = configuration.is_lifted;
  const float radius =
      configuration.list_style.kind() == ListStyle::Kind::grouped
          ? 0.0F
          : configuration.corner_radius.value_or(
                theme::metrics::list.grouped_corner_radius);
  composition::Element backdrop =
      fill(configuration, first || lifted, last || lifted, radius);
  composition::Element body =
      separated(
          row(configuration)
              .background(composition::Alignment::center, std::move(backdrop)),
          configuration, first || lifted)
          .frame({.max_width = composition::infinity});
  return lift(std::move(body), lifted);
}

auto CellStyle::plain(Configuration configuration) -> composition::Element {
  const bool first = configuration.is_first;
  const bool lifted = configuration.is_lifted;
  composition::Element body = row(configuration);
  if (configuration.background.has_value() && !lifted) {
    body = std::move(body).background(composition::Alignment::center,
                                      fill(configuration, true, true, 0.0F));
  } else {
    body = std::move(body).background(
        lifted ? theme::ColourValue{theme::ColourRole::surface_raised}
               : theme::ColourValue{clear});
  }
  const bool bordered =
      configuration.list_style.kind() == ListStyle::Kind::bordered;
  body = separated(std::move(body), configuration, first || lifted || bordered)
             .frame({.max_width = composition::infinity});
  return lift(std::move(body), lifted);
}

auto CellStyle::sidebar(Configuration configuration) -> composition::Element {
  const bool selected = configuration.is_selected;
  const bool lifted = configuration.is_lifted;
  composition::Element body = row(configuration);
  if (configuration.background.has_value() && !selected) {
    body = std::move(body).background(composition::Alignment::center,
                                      fill(configuration, true, true, 0.0F));
  }
  body = std::move(body).background(
      selected ? theme::ColourValue{theme::ColourRole::accent}
               : theme::ColourValue{clear},
      theme::Shape::rounded(theme::radius::control, true));
  if (selected) {
    body = std::move(body).foreground_style(theme::ColourRole::on_accent);
  }
  return lift(std::move(body), lifted);
}

auto DefaultCellStyle::make_body(CellStyle::Configuration configuration) const
    -> composition::Element {
  return CellStyle{*this}.make_body(std::move(configuration));
}

}
