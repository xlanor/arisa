#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/structure/tile_row.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <memory>
#include <utility>

namespace arisa::structure {

TileRow::TileRow(std::vector<Tile> tiles)
    : TileRow(std::move(tiles), Options{}) {}

TileRow::TileRow(std::vector<Tile> tiles, Options options)
    : Element(std::make_unique<Content>(std::move(tiles), options)) {}

auto TileRow::tile_key(std::size_t index) -> std::string {
  return std::format("{}.{}", tile_prefix, index);
}

auto TileRow::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TileRow::Content::tile(
    std::size_t index,
    const composition::FocusBinding<std::size_t>& focus) const
    -> composition::Element {
  const Tile& shown = tiles_[index];
  const theme::Shape shape = theme::Shape::rounded(tile_radius, true);
  const theme::ColourRole fill = shown.prominent
                                     ? theme::ColourRole::accent
                                     : theme::ColourRole::secondary_system_fill;
  composition::Element label =
      components::VStack(
          components::VStackOptions{.spacing = theme::Space{label_spacing}},
          components::Image(components::Image::SystemName{shown.system_image})
              .font(
                  theme::Font::system(icon_size, theme::Font::Weight::medium)),
          components::Text(shown.title)
              .font(options_.label_font)
              .multiline_text_alignment(composition::TextAlignment::center)
              .line_limit(options_.label_lines))
          .frame(
              {.width = options_.tile_width, .height = options_.tile_height});
  if (shown.prominent) {
    label = std::move(label).foreground_style(theme::ColourRole::on_accent);
  }
  return components::Button(
             [action = shown.action] {
               if (action) {
                 action();
               }
             },
             std::move(label)
                 .background(fill, shape)
                 .content_shape(composition::ContentShapeKinds::interaction,
                                shape))
      .button_style(components::ButtonStyle::plain())
      .content_shape(composition::ContentShapeKinds::focus_effect, shape)
      .focused(focus, index)
      .id(TileRow::tile_key(index));
}

auto TileRow::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const composition::FocusBinding<std::size_t> focus = context.state(tile_focus);
  std::vector<composition::Element> row;
  row.reserve(tiles_.size());
  for (std::size_t index = 0; index < tiles_.size(); ++index) {
    row.push_back(tile(index, focus));
  }
  const std::optional<std::size_t>& focused = focus.get();
  std::string hint;
  if (focused.has_value() && *focused < tiles_.size()) {
    hint = tiles_[*focused].hint;
  }
  composition::Element tiles =
      components::HStack(
          components::HStackOptions{.spacing = theme::Space{tile_spacing}},
          std::move(row))
          .focus_section();
  if (options_.default_focus.has_value() &&
      *options_.default_focus < tiles_.size()) {
    tiles = std::move(tiles).default_focus(focus, *options_.default_focus);
  }
  std::vector<composition::Element> produced;
  produced.push_back(components::VStack(
      components::VStackOptions{.spacing = theme::Space{hint_spacing}},
      std::move(tiles),
      components::Text(std::move(hint))
          .font(theme::Font::subheadline)
          .foreground_style(theme::ColourRole::secondary_label)
          .multiline_text_alignment(composition::TextAlignment::center)
          .line_limit(hint_lines, true)
          .frame({.max_width = composition::infinity})
          .id(std::string{hint_key})));
  return produced;
}

}
